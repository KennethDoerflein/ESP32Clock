// ntp.h

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

// Declare the sensor objects so other files know they exist
extern const char *NTP_SERVER;
extern const char *BACKUP_NTP_SERVER;
extern const char *BACKUP2_NTP_SERVER;
extern const char *TZ_INFO;

// Declare the functions
bool getNTPData(struct tm &timeinfo);
// Sync time with NTP servers. Returns true on success, false on failure.
bool syncTime();