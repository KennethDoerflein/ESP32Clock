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
#include "TimeManager.h"
#include "ConfigManager.h"
#include "LockGuard.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_sntp.h>

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

// --- Pre-sync snapshot variables (Layer 3: Three-Source Diagnostics) ---
/// @brief System clock epoch captured at the start of an NTP sync.
static time_t sysTimeAtSyncStart = 0;
/// @brief Hardware RTC epoch captured at the start of an NTP sync.
static uint32_t rtcTimeAtSyncStart = 0;
/// @brief millis() captured at the start of an NTP sync for elapsed time calculation.
static unsigned long millisAtSyncStart = 0;

/**
 * @brief Checks if a non-blocking NTP synchronization is currently in progress.
 * @return true if an NTP sync is in progress, false otherwise.
 */
bool isNtpSyncInProgress()
{
  LockGuard lock(ntpMutex);
  return ntpState == NTP_SYNC_IN_PROGRESS;
}

/**
 * @brief Processes the time data received from an NTP server.
 * @details Converts the tm struct to a DateTime and sets the hardware RTC.
 *          The ESP-IDF SNTP daemon already handles RTT compensation internally,
 *          so the time in `timeinfo` (from `getLocalTime`) is already accurate.
 * @param timeinfo The tm struct populated by a successful getLocalTime() call.
 */
static void _processSuccessfulNtpSync(const struct tm &timeinfo)
{
  auto &logger = SerialLog::getInstance();

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

  // --- Layer 3: Three-Source Diagnostic Dashboard ---
  // Back-calculate what each clock read at the moment the sync started,
  // using the elapsed time to compensate for network latency.
  if (millisAtSyncStart > 0 && sysTimeAtSyncStart > 0)
  {
    unsigned long elapsedMs = millis() - millisAtSyncStart;
    time_t ntpTimeAtStart = (time_t)time_to_set.unixtime() - (time_t)(elapsedMs / 1000);
    int32_t sysDrift = (int32_t)sysTimeAtSyncStart - (int32_t)ntpTimeAtStart;
    int32_t rtcDrift = (int32_t)rtcTimeAtSyncStart - (int32_t)ntpTimeAtStart;

    // Format the NTP reference time at sync start for logging
    struct tm ntpStartTm;
    time_t ntpStartEpoch = ntpTimeAtStart;
    gmtime_r(&ntpStartEpoch, &ntpStartTm);

    struct tm sysStartTm;
    gmtime_r(&sysTimeAtSyncStart, &sysStartTm);

    struct tm rtcStartTm;
    time_t rtcStartEpoch = (time_t)rtcTimeAtSyncStart;
    gmtime_r(&rtcStartEpoch, &rtcStartTm);

    logger.printf("\n");
    logger.printf("======= TIME CONSISTENCY CHECK =======\n");
    logger.printf("  NTP Reference : %04d-%02d-%02d %02d:%02d:%02d UTC (authoritative)\n",
                  ntpStartTm.tm_year + 1900, ntpStartTm.tm_mon + 1, ntpStartTm.tm_mday,
                  ntpStartTm.tm_hour, ntpStartTm.tm_min, ntpStartTm.tm_sec);
    logger.printf("  System Clock  : %04d-%02d-%02d %02d:%02d:%02d UTC  %+lds\n",
                  sysStartTm.tm_year + 1900, sysStartTm.tm_mon + 1, sysStartTm.tm_mday,
                  sysStartTm.tm_hour, sysStartTm.tm_min, sysStartTm.tm_sec, (long)sysDrift);
    logger.printf("  Hardware RTC  : %04d-%02d-%02d %02d:%02d:%02d UTC  %+lds\n",
                  rtcStartTm.tm_year + 1900, rtcStartTm.tm_mon + 1, rtcStartTm.tm_mday,
                  rtcStartTm.tm_hour, rtcStartTm.tm_min, rtcStartTm.tm_sec, (long)rtcDrift);
    logger.printf("  Sync Duration : %.2fs\n", elapsedMs / 1000.0);
    logger.printf("======================================\n");
  }
  else
  {
    // Fallback: no pre-sync snapshot (e.g., blocking sync at boot)
    DateTime oldRtcTime = TimeManager::getInstance().getRTCTime();
    if (oldRtcTime.isValid() && oldRtcTime.year() >= 2024)
    {
      int32_t driftSeconds = (int32_t)oldRtcTime.unixtime() - (int32_t)time_to_set.unixtime();
      logger.printf("NTP correction: RTC was %s by %ld seconds\n",
                    driftSeconds > 0 ? "ahead" : "behind",
                    (long)abs(driftSeconds));
    }
  }

  // --- Layer 4: RTC Write + Verification ---
  // Use TimeManager's mutex-protected method to prevent I2C bus corruption
  // from concurrent access by the main loop's getRTCTime() on Core 1.
  TimeManager::getInstance().adjustRTC(time_to_set);

  DateTime readback = TimeManager::getInstance().getRTCTime();
  int32_t writeError = abs((int32_t)readback.unixtime() - (int32_t)time_to_set.unixtime());
  if (writeError > 2)
  {
    logger.printf("WARNING: RTC write verification failed! Expected %lu, got %lu (delta=%ld)\n",
                  (unsigned long)time_to_set.unixtime(), (unsigned long)readback.unixtime(), (long)writeError);
  }

  // Update DST status in configuration (for UI/logging purposes only)
  ConfigManager::getInstance().setDST(timeinfo.tm_isdst > 0);

  logger.printf("RTC synchronized with NTP time (UTC): %04d-%02d-%02d %02d:%02d:%02d\n",
                time_to_set.year(), time_to_set.month(), time_to_set.day(),
                time_to_set.hour(), time_to_set.minute(), time_to_set.second());
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

  // ONLY return true if the background SNTP daemon has successfully received 
  // a packet from the network and updated the internal system clock. 
  // If we just check getLocalTime(), it will return true immediately even with 
  // the incorrect drifted time because we seeded the system clock from the RTC at boot.
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
  {
    return getLocalTime(&timeinfo, 0); // 0 timeout means don't block
  }
  return false;
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

  // --- Layer 3: Capture pre-sync snapshots for drift diagnostics ---
  // These are read back in _processSuccessfulNtpSync() to calculate
  // the precise drift of each clock source at the sync start moment.
  sysTimeAtSyncStart = time(nullptr);
  DateTime rtcSnap = TimeManager::getInstance().getRTCTime();
  rtcTimeAtSyncStart = rtcSnap.isValid() ? rtcSnap.unixtime() : 0;
  millisAtSyncStart = millis();

  // Reset the SNTP sync status to ensure we detect a new network update.
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  // Restart the SNTP client to force an immediate network query.
  configTime(0, 0, NTP_SERVER, BACKUP_NTP_SERVER, BACKUP2_NTP_SERVER);

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
  // Reset the SNTP sync status to ensure we detect a new network update.
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  // Restart the SNTP client to force an immediate network query.
  configTime(0, 0, NTP_SERVER, BACKUP_NTP_SERVER, BACKUP2_NTP_SERVER);

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