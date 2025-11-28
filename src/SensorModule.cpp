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
#include "driver/temp_sensor.h"
#include <math.h>

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME;
RTC_Type RTC;

// Static variables to cache the latest sensor readings.
static float cached_bme_temp_c = 0.0;
static float cached_rtc_temp_c = 0.0;
static float cached_core_temp_c = 0.0;
static float cached_humidity = 0.0;
static float cached_offset_c = 0.0;
static bool bme280_found = false; // Track BME280 sensor status
static unsigned long lastBmeRetry = 0;
const unsigned long BME_RETRY_INTERVAL = 10000; // Retry every 10 seconds if not found
const uint8_t BME280_I2C_ADDRESS = 0x76;

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
static bool rtc_found = false; // Track RTC status

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
    if (bme280_found)
    {
      float raw_bme_temp_c = BME.readTemperature();
      float raw_humidity = BME.readHumidity();

      // Check for sensor failure
      if (isnan(raw_bme_temp_c) || isnan(raw_humidity))
      {
        SerialLog::getInstance().print("BME280 read failed (NAN). Attempting to recover...");
        bme280_found = false;
        cached_humidity = -1;
      }
      else
      {
        if (rtc_found && ConfigManager::getInstance().isTempCorrectionEnabled())
        {
          float raw_rtc_temp_c = RTC.getTemperature();
          float correction = ConfigManager::getInstance().getTempCorrection();
          cached_offset_c = -((raw_rtc_temp_c - raw_bme_temp_c)) + correction;
          cached_bme_temp_c = raw_bme_temp_c + cached_offset_c;
          cached_humidity = calculateCorrectedHumidity(raw_bme_temp_c, raw_humidity, cached_offset_c);
        }
        else
        {
          // Correction is disabled or RTC is not found, use raw values
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

    if (rtc_found)
    {
      cached_rtc_temp_c = RTC.getTemperature();
    }
    temp_sensor_read_celsius(&cached_core_temp_c);
  }
}