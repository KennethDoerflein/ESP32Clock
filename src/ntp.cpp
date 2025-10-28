/**
 * @file ntp.cpp
 * @brief Implements NTP time synchronization logic.
 *
 * This file contains the implementation for both blocking and non-blocking
 * methods to synchronize the device's RTC with an NTP server. It uses
 * a robust retry mechanism with exponential backoff and jitter.
 */
#include "ntp.h"
#include "sensors.h"
#include "SerialLog.h"
#include "ConfigManager.h"
#include <Arduino.h>

// --- Common NTP Constants ---
/// @brief Primary NTP server address.
const char *NTP_SERVER = "time.nist.gov";
/// @brief First backup NTP server address.
const char *BACKUP_NTP_SERVER = "time.cloudflare.com";
/// @brief Second backup NTP server address.
const char *BACKUP2_NTP_SERVER = "us.pool.ntp.org";

/// @brief Maximum number of retry attempts for NTP synchronization.
const int maxRetries = 25;
/// @brief The base delay in milliseconds before the first retry.
const unsigned long baseDelayMs = 1000;
/// @brief The maximum delay in milliseconds between retries.
const unsigned long maxDelayMs = 30000;
/// @brief The maximum random jitter in milliseconds to add to delays.
const int jitterMaxMs = 1000;

// --- Non-Blocking NTP Sync State ---
/// @brief Current state of the non-blocking synchronization process.
static NtpSyncState ntpState = NTP_SYNC_IDLE;
/// @brief Counter for the number of retries in the current non-blocking sync.
static int retryCount = 0;
/// @brief Timestamp of the last non-blocking sync attempt.
static unsigned long lastSyncAttemptMs = 0;
/// @brief The current delay to wait before the next non-blocking retry.
static unsigned long currentRetryDelay = 0;

/**
 * @brief Processes the time data received from an NTP server.
 * @details This helper function converts the tm struct, compensates for delay,
 * adjusts the RTC, and prints a success message.
 * @param timeinfo The tm struct populated by a successful NTP call.
 */
static void _processSuccessfulNtpSync(const struct tm &timeinfo)
{
  // Convert the C `tm` struct to an `RTClib::DateTime` object.
  DateTime receivedTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec);

  // Compensate for potential network and processing delays.
  TimeSpan compensation(1); // 1 second
  DateTime adjustedTime = receivedTime + compensation;
  // Update the hardware RTC with the adjusted time.
  RTC.adjust(adjustedTime);

  SerialLog::getInstance().print("RTC synchronized with NTP time (compensated): ");
  char timeStr[20];
  sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
          adjustedTime.year(),
          adjustedTime.month(),
          adjustedTime.day(),
          adjustedTime.hour(),
          adjustedTime.minute(),
          adjustedTime.second());
  SerialLog::getInstance().printf("%s\n", timeStr);
}

/**
 * @brief Fetches time data from the configured NTP servers.
 */
bool getNTPData(struct tm &timeinfo)
{
  configTime(0, 0, NTP_SERVER, BACKUP_NTP_SERVER, BACKUP2_NTP_SERVER);
  setenv("TZ", ConfigManager::getInstance().getTimezone().c_str(), 1);
  tzset();
  // The getLocalTime function expects a pointer to the tm struct
  return getLocalTime(&timeinfo);
}

/**
 * @brief Starts the non-blocking NTP synchronization process.
 */
void startNtpSync()
{
  if (ntpState == NTP_SYNC_IN_PROGRESS)
  {
    return;
  }
  SerialLog::getInstance().printf("Starting non-blocking NTP sync...\n");
  ntpState = NTP_SYNC_IN_PROGRESS;
  retryCount = 0;
  // Set lastSyncAttemptMs to 0 to trigger an immediate first attempt in updateNtpSync
  lastSyncAttemptMs = 0;
  currentRetryDelay = baseDelayMs;
}

/**
 * @brief Updates the state of the non-blocking NTP synchronization.
 */
NtpSyncState updateNtpSync()
{
  if (ntpState != NTP_SYNC_IN_PROGRESS)
  {
    return ntpState;
  }

  unsigned long currentMillis = millis();
  // Check if it's time for the next attempt
  if (lastSyncAttemptMs != 0 && currentMillis - lastSyncAttemptMs < currentRetryDelay)
  {
    return NTP_SYNC_IN_PROGRESS; // Not time yet, still in progress
  }

  lastSyncAttemptMs = currentMillis; // Mark the time of this attempt
  retryCount++;

  SerialLog::getInstance().printf("Fetching NTP time (Attempt %d/%d)...\n", retryCount, maxRetries);

  struct tm timeinfo;
  // Pass the struct directly as the function expects a reference
  if (getNTPData(timeinfo))
  {
    _processSuccessfulNtpSync(timeinfo);
    ntpState = NTP_SYNC_SUCCESS; // Update state to success
    return ntpState;
  }

  // If sync failed, check for retry limit
  if (retryCount >= maxRetries)
  {
    SerialLog::getInstance().printf("Failed to sync time with NTP server after all retries.\n");
    ntpState = NTP_SYNC_FAILED; // Update state to failed
    return ntpState;
  }

  // Calculate delay for the next attempt with exponential backoff and jitter
  unsigned long jitter = random(jitterMaxMs + 1);
  unsigned long nextDelay = currentRetryDelay + jitter;

  SerialLog::getInstance().printf("Failed to obtain time. Retrying in approx. %.2f seconds...\n", nextDelay / 1000.0);

  // Exponentially increase the base delay for the *next* cycle
  currentRetryDelay *= 2;
  if (currentRetryDelay > maxDelayMs)
  {
    currentRetryDelay = maxDelayMs;
  }

  return NTP_SYNC_IN_PROGRESS; // Still in progress
}

/**
 * @brief Performs a blocking synchronization of the RTC with an NTP server.
 */
bool syncTime()
{
  unsigned long delayForNextAttempt = baseDelayMs;
  struct tm timeinfo;

  for (int i = 1; i <= maxRetries; i++)
  {
    SerialLog::getInstance().printf("Fetching NTP time (Attempt %d/%d)...\n", i, maxRetries);

    if (getNTPData(timeinfo))
    {
      _processSuccessfulNtpSync(timeinfo);
      return true;
    }

    if (i < maxRetries)
    {
      // Add a random jitter to the delay to prevent multiple devices from retrying in lockstep.
      unsigned long jitter = random(jitterMaxMs + 1);
      unsigned long totalDelay = delayForNextAttempt + jitter;

      SerialLog::getInstance().printf("Failed to obtain time. Retrying in %.2f seconds...\n", totalDelay / 1000.0);
      delay(totalDelay);

      delayForNextAttempt *= 2;
      if (delayForNextAttempt > maxDelayMs)
      {
        delayForNextAttempt = maxDelayMs;
      }
    }
  }

  SerialLog::getInstance().printf("Failed to sync time with NTP server after all retries.\n");
  return false;
}

/**
 * @brief Resets the state of the non-blocking NTP synchronization.
 */
void resetNtpSync()
{
  ntpState = NTP_SYNC_IDLE;
  SerialLog::getInstance().print("NTP sync state reset to IDLE.");
}

DateTime getNtpTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    SerialLog::getInstance().print("Failed to obtain NTP time.\n");
    return DateTime(); // Return an invalid DateTime
  }

  // Convert tm struct to DateTime object
  return DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec);
}