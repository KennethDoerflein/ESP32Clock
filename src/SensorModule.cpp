/**
 * @file SensorModule.cpp
 * @brief Implements sensor initialization and data handling.
 *
 * This file contains the implementation for setting up and reading data from
 * the BME280 environmental sensor, the DS3231 Real-Time Clock, and the
 * ESP32's internal temperature sensor. It uses a caching mechanism to avoid
 * reading the sensors too frequently.
 */

#include "SensorModule.h"
#include "ConfigManager.h"
#include "SerialLog.h"
#include "TimeManager.h"
#include "LockGuard.h"
#include "driver/temp_sensor.h"
#include <math.h>
#include "Display.h"

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME;
RTC_Type RTC;

// Static variables to cache the latest sensor readings.
static float cached_bme_temp_c = 0.0;
static float cached_raw_bme_temp_c = 0.0;
static float cached_rtc_temp_c = 0.0;
static float cached_core_temp_c = 0.0;
static float cached_humidity = 0.0;
static float cached_offset_c = 0.0;
static bool bme280_found = false; // Track BME280 sensor status
static unsigned long lastBmeRetry = 0;
const unsigned long BME_RETRY_INTERVAL = 10000; // Retry every 10 seconds if not found
const uint8_t BME280_I2C_ADDRESS = 0x76;

// EMA (Exponential Moving Average) state for smoothing the thermal delta.
static float smoothed_heat_delta = 0.0;
static bool ema_initialized = false;
const float EMA_ALPHA = 0.02; // Smoothing factor: lower = smoother, slower convergence (0.02 = ~15 min thermal inertia)

// EMA state for filtering BME280 raw sensor noise
static float smoothed_raw_bme_temp_c = 0.0;
static float smoothed_raw_humidity = 0.0;
static bool bme_ema_initialized = false;
const float BME_EMA_ALPHA = 0.2; // Fast smoothing for signal noise

/**
 * @brief Calculates the corrected relative humidity based on a temperature offset.
 *
 * This function uses the August-Roche-Magnus approximation to first calculate
 * the dew point from the original temperature and humidity, and then calculates
 * the new relative humidity at the adjusted temperature.
 *
 * @param temp_c The original temperature in Celsius.
 * @param humidity The original relative humidity.
 * @param offset_c The temperature offset in Celsius.
 * @return The corrected relative humidity.
 */
float calculateCorrectedHumidity(float temp_c, float humidity, float offset_c)
{
  if (humidity < 0 || humidity > 100)
  {
    return humidity; // Return original value if it's invalid
  }

  float temp_compensated_c = temp_c + offset_c;

  // August-Roche-Magnus approximation constants
  const float A = 17.625;
  const float B = 243.04;

  // Calculate dew point from original temperature and humidity
  float alpha = log(humidity / 100.0) + (A * temp_c) / (B + temp_c);
  float dew_point = (B * alpha) / (A - alpha);

  // Calculate new saturation vapor pressure at the compensated temperature
  float compensated_svp = exp((A * temp_compensated_c) / (B + temp_compensated_c));

  // Calculate saturation vapor pressure at the dew point
  float actual_vp = exp((A * dew_point) / (B + dew_point));

  // Calculate new relative humidity
  float new_humidity = 100.0 * (actual_vp / compensated_svp);

  // Clamp the result to a valid range
  if (new_humidity > 100.0)
  {
    new_humidity = 100.0;
  }
  else if (new_humidity < 0.0)
  {
    new_humidity = 0.0;
  }

  return new_humidity;
}
static bool rtc_found = false;        // Track RTC status
static bool core_temp_started = false; // Track whether temp_sensor_start() was called

// Auto-Calibration State
static bool auto_cal_running = false;
static unsigned long auto_cal_start_time = 0;
static float auto_cal_baseline_temp = 0.0;
static bool first_run_check_done = false;

/// @brief Stores the timestamp of the last sensor update for interval timing.
static unsigned long prevSensorMillis = 0;

/**
 * @brief Initializes all hardware sensors.
 *
 * This function attempts to initialize the BME280 sensor and the RTC.
 * It also sets up the ESP32's internal temperature sensor. It then performs
 * an initial sensor read to populate the cached values.
 */
void setupSensors()
{
  for (int i = 0; i < SENSOR_RETRY_COUNT; ++i)
  {
    bme280_found = BME.begin(BME280_I2C_ADDRESS);
    if (bme280_found)
      break;
    delay(SENSOR_RETRY_DELAY);
  }

  if (!bme280_found)
  {
    SerialLog::getInstance().print("Could not find a valid BME280 sensor, check wiring!");
    // The device will now rely on the RTC for temperature.
  }

  for (int i = 0; i < SENSOR_RETRY_COUNT; ++i)
  {
    rtc_found = RTC.begin();
    if (rtc_found)
      break;
    delay(SENSOR_RETRY_DELAY);
  }

  if (!rtc_found)
  {
    SerialLog::getInstance().print("Couldn't find RTC");
    // The main loop will now handle the error message.
    return; // Exit early, no point in continuing
  }

  // Initialize the internal temperature sensor
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();
  core_temp_started = true;

  // Perform an initial sensor read to populate cached values.
  handleSensorUpdates(true);
}

/**
 * @brief Checks if the RTC module was found.
 * @return True if the RTC is available, false otherwise.
 */
bool isRtcFound()
{
  return rtc_found;
}

/**
 * @brief Checks if the BME280 sensor was found.
 * @return True if the BME280 is available, false otherwise.
 */
bool isBmeFound()
{
  return bme280_found;
}

/**
 * @brief Gets the primary temperature reading.
 *
 * Returns the BME280 temperature if available, otherwise falls back to the
 * RTC temperature. The value is converted to the user's preferred unit (C/F).
 *
 * @return The current temperature.
 */
float getTemperature()
{
  if (bme280_found)
  {
    return getBmeTemperature();
  }
  else
  {
    return getRtcTemperature();
  }
}

/**
 * @brief Gets the last cached humidity reading.
 * @return The relative humidity, or -1 if the BME280 is not available.
 */
float getHumidity()
{
  return cached_humidity;
}

/**
 * @brief Gets the BME280 temperature, converted to the user's preferred unit.
 * @return The BME280 temperature.
 */
float getBmeTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_bme_temp_c;
  }
  else
  {
    return (cached_bme_temp_c * 9.0 / 5.0) + 32.0;
  }
}

/**
 * @brief Gets the RTC's internal temperature, converted to the user's preferred unit.
 * @return The RTC temperature.
 */
float getRtcTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_rtc_temp_c;
  }
  else
  {
    return (cached_rtc_temp_c * 9.0 / 5.0) + 32.0;
  }
}

/**
 * @brief Gets the ESP32's internal core temperature, converted to the user's preferred unit.
 * @return The core temperature.
 */
float getCoreTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_core_temp_c;
  }
  else
  {
    return (cached_core_temp_c * 9.0 / 5.0) + 32.0;
  }
}

/**
 * @brief Gets the raw (uncompensated) BME280 temperature, converted to the user's preferred unit.
 * @return The raw BME280 temperature, or 0 if the BME280 is not available.
 */
float getRawBmeTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_raw_bme_temp_c;
  }
  else
  {
    return (cached_raw_bme_temp_c * 9.0 / 5.0) + 32.0;
  }
}

/**
 * @brief Gets the total compensation offset currently being applied.
 * @return The offset in the user's preferred unit (negative = temperature lowered).
 */
float getCompensationOffset()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_offset_c;
  }
  else
  {
    return cached_offset_c * 9.0 / 5.0;
  }
}

/**
 * @brief Checks if the cold-boot auto-calibration is currently in progress.
 * @return True if calibrating, false otherwise.
 */
bool isAutoCalibrating()
{
  return auto_cal_running;
}

/**
 * @brief Periodically reads sensor data and updates the cache.
 *
 * This function is designed to be called in the main loop. It uses a timer
 * to read from the sensors at a defined interval, preventing excessive I2C
 * traffic.
 *
 * @param force If true, forces an immediate sensor read, ignoring the timer.
 */
void handleSensorUpdates(bool force)
{
  unsigned long now = millis();
  if (force || (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL))
  {
    prevSensorMillis = now;

    // All I2C operations (BME280 and RTC temperature reads) must be
    // serialized with the same mutex that TimeManager uses for RTC.now()
    // and RTC.adjust(). Without this, loopTask (Core 1) and logicTask
    // (Core 0) can access the Wire bus simultaneously, corrupting the
    // I2C transaction state and triggering a FreeRTOS mutex assertion.
    RecursiveLockGuard i2cLock(TimeManager::getInstance().getI2CMutex());

    // Read the core temp sensor FIRST so it's available for compensation.
    if (core_temp_started)
    {
      float temp_c = 0.0f;
      if (temp_sensor_read_celsius(&temp_c) == ESP_OK)
      {
        cached_core_temp_c = temp_c;
      }
      else
      {
        SerialLog::getInstance().print("WARNING: Failed to read internal core temperature\n");
      }
    }

    // Read RTC temperature early to use it in compensation logic as an enclosure ambient proxy
    if (rtc_found)
    {
      cached_rtc_temp_c = RTC.getTemperature();
    }

    if (bme280_found)
    {
      float raw_bme_temp_c = BME.readTemperature();
      float raw_humidity = BME.readHumidity();

      // --- Cold-Boot Auto-Calibration Initialization ---
      if (!first_run_check_done && core_temp_started && rtc_found)
      {
        first_run_check_done = true;
        if (ConfigManager::getInstance().isAutoTempCalibration())
        {
          float initial_diff = std::abs(cached_rtc_temp_c - raw_bme_temp_c);
          if (initial_diff < 0.5f)
          {
            auto_cal_running = true;
            auto_cal_baseline_temp = raw_bme_temp_c;
            auto_cal_start_time = millis();
            String startedStr = "Started at " + TimeManager::getInstance().getCachedTime().timestamp(DateTime::TIMESTAMP_FULL);
            ConfigManager::getInstance().setLastAutoCalStatus(startedStr);
            SerialLog::getInstance().print("Cold Boot Detected: Auto-Calibration started.");
          }
        }
      }

      // Check for sensor failure
      if (isnan(raw_bme_temp_c) || isnan(raw_humidity))
      {
        SerialLog::getInstance().print("BME280 read failed (NAN). Attempting to recover...");
        bme280_found = false;
        cached_humidity = -1;
      }
      else
      {
        // Apply EMA filter to raw BME280 readings to reduce Gaussian noise
        if (!bme_ema_initialized)
        {
          smoothed_raw_bme_temp_c = raw_bme_temp_c;
          smoothed_raw_humidity = raw_humidity;
          bme_ema_initialized = true;
        }
        else
        {
          smoothed_raw_bme_temp_c = BME_EMA_ALPHA * raw_bme_temp_c + (1.0f - BME_EMA_ALPHA) * smoothed_raw_bme_temp_c;
          smoothed_raw_humidity = BME_EMA_ALPHA * raw_humidity + (1.0f - BME_EMA_ALPHA) * smoothed_raw_humidity;
        }
        
        // Use the smoothed raw readings for all subsequent math
        raw_bme_temp_c = smoothed_raw_bme_temp_c;
        raw_humidity = smoothed_raw_humidity;

        // Always cache the raw reading for diagnostics
        cached_raw_bme_temp_c = raw_bme_temp_c;

        if (core_temp_started && ConfigManager::getInstance().isTempCorrectionEnabled())
        {
          // --- Core and RTC-based thermal compensation ---
          // The ESP32 core temp tracks rapid internal heat generation (CPU load).
          // The RTC temp acts as a proxy for the general enclosure air temp (warmed by display).
          // We blend them to create an effective internal heat source temperature.
          // Since the RTC is closer to the ESP32 than the BME, the RTC acts as a great
          // physical proxy for the "hot zone" air temperature. We weight it heavily.
          float effective_internal_temp = cached_core_temp_c;
          if (rtc_found && cached_rtc_temp_c > 0.0f)
          {
            effective_internal_temp = (cached_core_temp_c * 0.25f) + (cached_rtc_temp_c * 0.75f);
          }

          float heat_delta = effective_internal_temp - raw_bme_temp_c;

          // --- Brightness-Scaled Thermal Compensation ---
          // The display backlight is the primary heat source. When it dims or turns off, 
          // its heating effect on the BME280 decreases significantly. We scale the 
          // calculated heat delta by the current brightness ratio. 
          // Based on empirical thermal tuning from 4 data points, the ESP32 core accounts 
          // for roughly 80% of the compensation gap at night due to the difference in cooling 
          // rates between the internal RTC and external BME.
          int current_brightness = Display::getInstance().getActualBrightness();
          float brightness_ratio = current_brightness / 255.0f;
          float adjusted_heat_delta = heat_delta * (0.80f + 0.20f * brightness_ratio);

          // Apply EMA smoothing to the adjusted heat delta to simulate enclosure thermal mass
          if (!ema_initialized)
          {
            smoothed_heat_delta = adjusted_heat_delta;
            ema_initialized = true;
          }
          else
          {
            smoothed_heat_delta = EMA_ALPHA * adjusted_heat_delta + (1.0f - EMA_ALPHA) * smoothed_heat_delta;
          }

          // --- Cold-Boot Auto-Calibration Finalization ---
          if (auto_cal_running)
          {
            if (millis() - auto_cal_start_time > 2700000) // 45 minutes
            {
              if (smoothed_heat_delta > 0.5f)
              {
                float new_k = (raw_bme_temp_c - auto_cal_baseline_temp) / smoothed_heat_delta;
                if (new_k < 0.0f) new_k = 0.0f;
                if (new_k > 1.0f) new_k = 1.0f;
                ConfigManager::getInstance().setTempCompensationFactor(new_k);
                String successStr = "Success: k=" + String(new_k, 2) + " at " + TimeManager::getInstance().getCachedTime().timestamp(DateTime::TIMESTAMP_FULL);
                ConfigManager::getInstance().setLastAutoCalStatus(successStr);
                SerialLog::getInstance().print("Auto-Calibration finished. New factor applied.");
              }
              else
              {
                String abortedStr = "Aborted: low heat at " + TimeManager::getInstance().getCachedTime().timestamp(DateTime::TIMESTAMP_FULL);
                ConfigManager::getInstance().setLastAutoCalStatus(abortedStr);
                SerialLog::getInstance().print("Auto-Calibration aborted: Insufficient heat buildup.");
              }
              auto_cal_running = false;
            }
          }

          float k = ConfigManager::getInstance().getTempCompensationFactor();
          float user_correction = ConfigManager::getInstance().getTempCorrection();

          // Estimated self-heating: fraction of the smoothed thermal gradient
          // Clamped to >= 0 because negative self-heat is physically nonsensical
          float self_heat = k * smoothed_heat_delta;
          if (self_heat < 0.0f)
          {
            self_heat = 0.0f;
          }

          // Total offset: subtract estimated self-heating, add manual correction
          cached_offset_c = -self_heat + user_correction;
          cached_bme_temp_c = raw_bme_temp_c + cached_offset_c;
          cached_humidity = calculateCorrectedHumidity(raw_bme_temp_c, raw_humidity, cached_offset_c);
        }
        else
        {
          // Correction is disabled or core temp sensor not available, use raw values
          cached_bme_temp_c = raw_bme_temp_c;
          cached_humidity = raw_humidity;
          cached_offset_c = 0.0;
        }
      }
    }

    if (!bme280_found)
    {
      cached_humidity = -1; // Indicate that humidity is not available
      if (now - lastBmeRetry >= BME_RETRY_INTERVAL)
      {
        lastBmeRetry = now;
        SerialLog::getInstance().print("Attempting to reconnect BME280...");
        if (BME.begin(BME280_I2C_ADDRESS))
        {
          SerialLog::getInstance().print("BME280 recovered!");
          bme280_found = true;
        }
      }
    }

  }
}