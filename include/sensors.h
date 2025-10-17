// sensors.h

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

/**
 * @file sensors.h
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
 * @brief Prints the current BME280 sensor data to the Serial monitor.
 * @param useCelsius True to print temperature in Celsius, false for Fahrenheit.
 */
void printBME280Data(bool useCelsius);

/**
 * @brief Reads the current temperature directly from the BME280 sensor.
 * @param useCelsius True to return the temperature in Celsius, false for Fahrenheit.
 * @return The current temperature as a float.
 */
float readTemperature(bool useCelsius);

/**
 * @brief Reads the current humidity directly from the BME280 sensor.
 * @return The current relative humidity as a float.
 */
float readHumidity();

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