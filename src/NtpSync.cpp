/**
 * @file ntp.cpp
 * @brief Implements NTP time synchronization logic.
 *
 * This file contains the implementation for both blocking and non-blocking
 * methods to synchronize the device's RTC with an NTP server. It uses
 * a robust retry mechanism with exponential backoff and jitter.
 */
#include "NtpSync.h"
#include "SensorModule.h"
#include "SerialLog.h"
#include "ConfigManager.h"
#include "LockGuard.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

/**
 * @brief One-time initialization of the SNTP client.
 * @details Configures the NTP servers and timezone. This should be called
 * once at startup. The SNTP daemon runs in the background and keeps the
 * ESP32's system clock accurate; subsequent calls to getLocalTime() will
 * return the compensated time directly.
 */
void initNtp()
{
  configTime(0, 0, NTP_SERVER, BACKUP_NTP_SERVER, BACKUP2_NTP_SERVER);
  setenv("TZ", ConfigManager::getInstance().getTimezone().c_str(), 1);
  tzset();
  SerialLog::getInstance().print("NTP: SNTP client initialized.\n");
}

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
/// @brief Mutex protecting all NTP sync state (accessed from async web task and Core 0 logicTask).
static SemaphoreHandle_t ntpMutex = xSemaphoreCreateMutex();
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
 * @details Converts the tm struct to a DateTime and sets the hardware RTC.
 *          The ESP-IDF SNTP daemon already handles RTT compensation internally,
 *          so the time in `timeinfo` (from `getLocalTime`) is already accurate.
 * @param timeinfo The tm struct populated by a successful getLocalTime() call.
 */
static void _processSuccessfulNtpSync(const struct tm &timeinfo)
{
  // The system clock is already synced by SNTP. 
  // We want to store UTC in the hardware RTC.
  time_t now = time(nullptr);
  struct tm utc_tm;
  gmtime_r(&now, &utc_tm);

  DateTime time_to_set(
      utc_tm.tm_year + 1900,
      utc_tm.tm_mon + 1,
      utc_tm.tm_mday,
      utc_tm.tm_hour,
      utc_tm.tm_min,
      utc_tm.tm_sec);

  // Update the hardware RTC with UTC time.
  RTC.adjust(time_to_set);

  // Update DST status in configuration (for UI/logging purposes only)
  ConfigManager::getInstance().setDST(timeinfo.tm_isdst > 0);

  SerialLog::getInstance().print("RTC synchronized with NTP time (UTC): ");
  char timeStr[25];
  sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d UTC",
          time_to_set.year(),
          time_to_set.month(),
          time_to_set.day(),
          time_to_set.hour(),
          time_to_set.minute(),
          time_to_set.second());
  SerialLog::getInstance().printf("%s\n", timeStr);
}

/**
 * @brief Fetches time data from the configured NTP servers.
 * @details Reads the current system time (kept accurate by the SNTP daemon
 *          initialized via initNtp()). Refreshes the timezone in case the
 *          user changed it since the last call.
 */
bool getNTPData(struct tm &timeinfo)
{
  // Refresh timezone in case user changed it
  setenv("TZ", ConfigManager::getInstance().getTimezone().c_str(), 1);
  tzset();
  return getLocalTime(&timeinfo);
}

/**
 * @brief Starts the non-blocking NTP synchronization process.
 */
void startNtpSync()
{
  LockGuard lock(ntpMutex);
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
  LockGuard lock(ntpMutex);
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

    esp_task_wdt_reset(); // Feed before the potentially blocking getNTPData
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
      
      // Use a loop for the delay to keep the watchdog fed if the delay is long
      unsigned long startDelay = millis();
      while (millis() - startDelay < totalDelay) {
        esp_task_wdt_reset();
        delay(10);
      }

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
  LockGuard lock(ntpMutex);
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

  // The system clock returns local time via getLocalTime().
  // Convert to UTC to match the RTC (which stores UTC).
  time_t now = time(nullptr);
  struct tm utc_tm;
  gmtime_r(&now, &utc_tm);

  return DateTime(
      utc_tm.tm_year + 1900,
      utc_tm.tm_mon + 1,
      utc_tm.tm_mday,
      utc_tm.tm_hour,
      utc_tm.tm_min,
      utc_tm.tm_sec);
}