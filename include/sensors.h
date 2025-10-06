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
 * initialize them and read data from them. It includes conditional compilation
 * to support different RTC modules for simulation (Wokwi) and physical hardware.
 */

/// @brief Global instance of the BME280 sensor driver.
extern Adafruit_BME280 BME;

// Conditionally define the RTC type based on the build environment.
// Wokwi's simulator uses a DS1307, while the physical hardware uses a DS3231.
#ifdef WOKWI
using RTC_Type = RTC_DS1307;
#else
using RTC_Type = RTC_DS3231;
#endif

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

/**
 * @brief Prints the current BME280 sensor data to the Serial monitor.
 * @param useCelsius True to print temperature in Celsius, false for Fahrenheit.
 */
void printBME280Data(bool useCelsius);

/**
 * @brief Reads the current temperature from the BME280 sensor.
 * @param useCelsius True to return the temperature in Celsius, false for Fahrenheit.
 * @return The current temperature as a float.
 */
float readTemperature(bool useCelsius);

/**
 * @brief Reads the current humidity from the BME280 sensor.
 * @return The current relative humidity as a float.
 */
float readHumidity();