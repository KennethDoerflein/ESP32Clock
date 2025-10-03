// sensors.h

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

// Declare the sensor objects so other files know they exist
extern Adafruit_BME280 BME;

// RTC type differs between simulator (Wokwi uses DS1307) and hardware (DS3231).
#ifdef WOKWI
using RTC_Type = RTC_DS1307;
#else
using RTC_Type = RTC_DS3231;
#endif

extern RTC_Type RTC; // Single extern declaration using alias

// Declare the functions
void setupSensors();
void printBME280Data(bool useCelsius);
float readTemperature(bool useCelsius);
float readHumidity();