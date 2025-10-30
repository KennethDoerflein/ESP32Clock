#pragma once
#include <RTClib.h>
/**
 * @file NtpSync.h
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
 * @brief Fetches time data from the configured NTP servers.
 *
 * This is a low-level function that configures the ESP32's time client
 * and attempts to retrieve the current time. It populates a `tm` struct
 * with the results. This function can block while waiting for a network response.
 *
 * @param timeinfo A reference to a `tm` struct to be populated with the time data.
 * @return true if the time was successfully obtained.
 * @return false if the time could not be obtained.
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

/**
 * @brief Represents the state of the non-blocking NTP synchronization process.
 */
enum NtpSyncState
{
  NTP_SYNC_IDLE,        ///< The NTP client is idle and not currently syncing.
  NTP_SYNC_IN_PROGRESS, ///< An NTP synchronization is currently in progress.
  NTP_SYNC_SUCCESS,     ///< The last NTP synchronization attempt was successful.
  NTP_SYNC_FAILED       ///< The last NTP synchronization attempt failed after all retries.
};

bool getNTPData(struct tm &timeinfo);

/**
 * @brief Starts the non-blocking NTP synchronization process.
 *
 * This function initializes the state machine for a non-blocking sync.
 * It should be called once to begin the process. The actual synchronization
 * attempts are handled by calling `updateNtpSync()` repeatedly in the main loop.
 */
void startNtpSync();

/**
 * @brief Updates the state of the non-blocking NTP synchronization.
 *
 * This function should be called repeatedly in the main application loop.
 * It manages the state of the sync process, including making attempts,
 * handling retries with exponential backoff, and setting the final state.
 * It does not block execution.
 *
 * @return The current state of the NTP synchronization process (`NtpSyncState`).
 */
NtpSyncState updateNtpSync();

/**
 * @brief Resets the state of the non-blocking NTP synchronization.
 *
 * This function sets the NTP sync state back to IDLE. This is useful
 * after a sync attempt has completed (either successfully or not) to prevent
 * the system from getting stuck in a SUCCESS or FAILED state.
 */
void resetNtpSync();

/**
 * @brief Fetches the current time from an NTP server without setting the RTC.
 *
 * This function retrieves the time from an NTP server and returns it as a
 * DateTime object. It is useful for checking the current time without
 * modifying the system's RTC.
 *
 * @return A DateTime object representing the current NTP time. If the sync
 *         fails, the returned object will be invalid (`!isValid()`).
 */
DateTime getNtpTime();