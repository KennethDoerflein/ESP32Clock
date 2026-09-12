#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>
#include <math.h>

/// @brief The number of times to retry sensor initialization.
const int SENSOR_RETRY_COUNT = 3;

/// @brief The delay between sensor initialization retries, in milliseconds.
const int SENSOR_RETRY_DELAY = 250;

/**
 * @file SensorModule.h
 * @brief Manages hardware sensors like the BME280 and the RTC.
 *
 * This file declares the global sensor objects and the functions used to
 * initialize them and read data from them.
 */

/// @brief Global instance of the BME280 sensor driver.
extern Adafruit_BME280 BME;

// Define the RTC type for the physical hardware (DS3231).
using RTC_Type = RTC_DS3231;

/// @brief Global instance of the RTC driver, using the conditionally defined type.
extern RTC_Type RTC;

/**
 * @brief Initializes all connected hardware sensors.
 *
 * This function sets up the I2C communication and initializes the BME280
 * temperature/humidity sensor and the DS3231/DS1307 Real-Time Clock.
 * It should be called once during setup.
 */
void setupSensors();

/// @brief The interval for reading sensor data, in milliseconds.
const unsigned long SENSOR_UPDATE_INTERVAL = 5000; // 5 seconds

/**
 * @brief Handles periodic reading of sensor data.
 *
 * This function checks if the update interval has passed and, if so,
 * reads the latest data from the sensors and updates a local cache.
 * It is designed to be called continuously from the main loop.
 * @param force If true, forces an immediate sensor read, ignoring the interval.
 */
void handleSensorUpdates(bool force = false);

/**
 * @brief Checks if the RTC module was successfully initialized.
 * @return True if the RTC is available, false otherwise.
 */
bool isRtcFound();

/**
 * @brief Gets the last cached temperature reading.
 * @return The cached temperature, converted to the user's preferred unit.
 */
float getTemperature();

/**
 * @brief Gets the last cached humidity reading.
 * @return The cached relative humidity as a float.
 */
float getHumidity();

/**
 * @brief Checks if the BME280 sensor was successfully initialized.
 * @return True if the BME280 is available, false otherwise.
 */
bool isBmeFound();

/**
 * @brief Gets the last cached temperature reading from the BME280.
 * @return The cached temperature, converted to the user's preferred unit.
 */
float getBmeTemperature();

/**
 * @brief Gets the last cached temperature reading from the RTC.
 * @return The cached temperature, converted to the user's preferred unit.
 */
float getRtcTemperature();

/**
 * @brief Gets the last cached temperature reading from the ESP32-S3's internal sensor.
 * @return The cached temperature, converted to the user's preferred unit.
 */
float getCoreTemperature();

/**
 * @brief Gets the last cached raw (uncompensated) BME280 temperature reading.
 * @return The raw temperature, converted to the user's preferred unit.
 */
float getRawBmeTemperature();

/**
 * @brief Gets the total compensation offset currently being applied.
 * @return The offset in the user's preferred unit (negative = cooling correction).
 */
float getCompensationOffset();

/**
 * @brief Checks if the cold-boot auto-calibration is currently in progress.
 * @return True if calibrating, false otherwise.
 */
bool isAutoCalibrating();

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
inline float celsiusToFahrenheit(float c)
{
  return (c * 9.0f / 5.0f) + 32.0f;
}

inline float fahrenheitToCelsius(float f)
{
  return (f - 32.0f) * 5.0f / 9.0f;
}

inline float calculateCorrectedHumidity(float temp_c, float humidity, float offset_c)
{
  if (humidity < 0.0f || humidity > 100.0f)
  {
    return humidity; // Return original value if it's invalid
  }

  // Zero humidity means zero moisture; relative humidity remains 0%.
  // Also avoids log(0) producing -inf and subsequently NaN.
  if (humidity <= 0.0f)
  {
    return 0.0f;
  }

  if (offset_c == 0.0f)
  {
    return humidity;
  }

  float temp_compensated_c = temp_c + offset_c;

  // August-Roche-Magnus approximation constants
  const float A = 17.625f;
  const float B = 243.04f;

  // Calculate dew point from original temperature and humidity
  float alpha = log(humidity / 100.0f) + (A * temp_c) / (B + temp_c);
  float dew_point = (B * alpha) / (A - alpha);

  // Calculate new saturation vapor pressure at the compensated temperature
  float compensated_svp = exp((A * temp_compensated_c) / (B + temp_compensated_c));

  // Calculate saturation vapor pressure at the dew point
  float actual_vp = exp((A * dew_point) / (B + dew_point));

  // Calculate new relative humidity
  float new_humidity = 100.0f * (actual_vp / compensated_svp);

  // Safety check for NaN / Inf
  if (isnan(new_humidity) || isinf(new_humidity))
  {
    return humidity;
  }

  // Clamp the result to a valid range
  if (new_humidity > 100.0f)
  {
    new_humidity = 100.0f;
  }
  else if (new_humidity < 0.0f)
  {
    new_humidity = 0.0f;
  }

  return new_humidity;
}