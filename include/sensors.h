// sensors.h

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

// Declare the sensor objects so other files know they exist
extern Adafruit_BME280 bme;
extern RTC_DS3231 RTC;

// Declare the functions
void setupSensors();
void printBME280Data();