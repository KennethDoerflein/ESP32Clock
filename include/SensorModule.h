#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

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
const unsigned long SENSOR_UPDATE_INTERVAL = 3000; // 3 seconds

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