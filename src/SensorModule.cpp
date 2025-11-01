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
#include "driver/temp_sensor.h"

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME;
RTC_Type RTC;

// Static variables to cache the latest sensor readings.
static float cached_bme_temp_c = 0.0;
static float cached_rtc_temp_c = 0.0;
static float cached_core_temp_c = 0.0;
static float cached_humidity = 0.0;
static bool bme280_found = false; // Track BME280 sensor status
static bool rtc_found = false;    // Track RTC status

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
    bme280_found = BME.begin(0x76);
    if (bme280_found)
      break;
    delay(SENSOR_RETRY_DELAY);
  }

  if (!bme280_found)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
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
    Serial.println("Couldn't find RTC");
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
      cached_bme_temp_c = BME.readTemperature();
      cached_humidity = BME.readHumidity();
    }
    else
    {
      cached_humidity = -1; // Indicate that humidity is not available
    }

    if (rtc_found)
    {
      cached_rtc_temp_c = RTC.getTemperature();
    }
    temp_sensor_read_celsius(&cached_core_temp_c);
  }
}