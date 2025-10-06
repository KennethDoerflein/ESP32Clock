// ntp.h

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_BME280.h>

/**
 * @file ntp.h
 * @brief Handles Network Time Protocol (NTP) synchronization.
 *
 * This file declares the constants and functions required to fetch time
 * from NTP servers and synchronize the device's Real-Time Clock (RTC).
 */

/// @brief Primary NTP server address.
extern const char *NTP_SERVER;
/// @brief Secondary (backup) NTP server address.
extern const char *BACKUP_NTP_SERVER;
/// @brief Tertiary (backup) NTP server address.
extern const char *BACKUP2_NTP_SERVER;
/// @brief Timezone information string for POSIX `setenv`.
extern const char *TZ_INFO;

/**
 * @brief Fetches the current time from an NTP server.
 * @param timeinfo A `tm` struct to be filled with the fetched time data.
 * @return True if the time was fetched successfully, false otherwise.
 */
bool getNTPData(struct tm &timeinfo);

/**
 * @brief Synchronizes the device's RTC with the time from an NTP server.
 *
 * This function attempts to get the time using `getNTPData` and, if successful,
 * updates the RTC with this time.
 * @return True if the RTC was synchronized successfully, false otherwise.
 */
bool syncTime();