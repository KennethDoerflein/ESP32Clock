// sensors.h

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

// Declare the sensor objects so other files know they exist
extern Adafruit_BME280 bme;
// extern RTC_DS3231 RTC;
extern RTC_DS1307 RTC; // Testing with Wokwi DS1307 module

// Declare the functions
void setupSensors();
void printBME280Data();