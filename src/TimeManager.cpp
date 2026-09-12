/**
 * @file TimeManager.cpp
 * @brief Implements the TimeManager class for timekeeping and synchronization.
 *
 * This file contains the implementation for managing the device's time,
 * including RTC communication, NTP synchronization, time formatting, and
 * alarm checking logic.
 */

// #define LOG_TICKS

#include "TimeManager.h"
#include "HardwareBus.h"
#include "NtpSync.h"
#include "SensorModule.h"
#include "ConfigManager.h"
#include "AlarmManager.h"
#include "SerialLog.h"
#include "LockGuard.h"
#include <ctime>
#include <sys/time.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <vector>
#include <algorithm>

DateTime calculateNextRingTime(const Alarm &alarm, const DateTime &now)
{
  if (!alarm.isEnabled())
  {
    return DateTime(); // Return invalid DateTime
  }

  // If the alarm is snoozed, the next ring time is the snooze end time.
  if (alarm.isSnoozed())
  {
    time_t snoozeEpoch = alarm.getSnoozeUntil();
    struct tm t_local;
    {
      RecursiveLockGuard lock(TimeManager::getInstance().getStateMutex());
      if (localtime_r(&snoozeEpoch, &t_local) == nullptr)
      {
        return DateTime();
      }
    }
    return DateTime(t_local.tm_year + 1900, t_local.tm_mon + 1, t_local.tm_mday,
                    t_local.tm_hour, t_local.tm_min, t_local.tm_sec);
  }

  // Check today
  if (alarm.getHour() > now.hour() || (alarm.getHour() == now.hour() && alarm.getMinute() > now.minute()))
  {
    // If it's a repeating alarm and we already dismissed it today, don't schedule it for today!
    if (alarm.getDays() != 0 && alarm.getLastDismissedDayEpoch() == now.unixtime() / 86400)
    {
      // Skip today, fall through to future days
    }
    else if (alarm.getDays() == 0 || (alarm.getDays() & (1 << now.dayOfTheWeek())))
    {
      DateTime candidate(now.year(), now.month(), now.day(), alarm.getHour(), alarm.getMinute(), 0);
      // For biweekly alarms, verify this is the correct week
      if (!alarm.isBiweekly() || Alarm::isOddWeek(candidate) == alarm.isBiweeklyOddWeek())
      {
        return candidate;
      }
    }
  }

  // Check future days (14 days for biweekly to find the correct week parity)
  int maxDays = alarm.isBiweekly() ? 14 : 7;
  for (int i = 1; i <= maxDays; ++i)
  {
    uint8_t dayOfWeek = (now.dayOfTheWeek() + i) % 7;
    if (alarm.getDays() == 0 || (alarm.getDays() & (1 << dayOfWeek)))
    {
      DateTime next = now + TimeSpan(i, 0, 0, 0);
      DateTime candidate(next.year(), next.month(), next.day(), alarm.getHour(), alarm.getMinute(), 0);
      // For biweekly alarms, verify this is the correct week
      if (!alarm.isBiweekly() || Alarm::isOddWeek(candidate) == alarm.isBiweeklyOddWeek())
      {
        return candidate;
      }
    }
  }

  return DateTime(); // No valid ring time found
}

TimeManager::TimeManager()
{
  _mutex = xSemaphoreCreateRecursiveMutex();
}

/**
 * @brief Seeds the ESP32's internal system clock from the hardware RTC.
 *
 * The ESP32's time() function returns epoch 0 on every boot until SNTP
 * succeeds. This creates a window where all timestamps are wrong and
 * localtime_r returns Jan 1, 1970. By seeding from the DS3231 hardware
 * RTC immediately at boot, we ensure:
 * - SerialLog timestamps show correct wallclock time from the first line
 * - checkDST()/getLocalTime() work correctly before NTP
 * - If NTP fails entirely, the clock still shows the right time
 */
void TimeManager::seedSystemClockFromRTC()
{
  auto &logger = SerialLog::getInstance();

  DateTime rtcTime = getRTCTime();
  if (!rtcTime.isValid() || rtcTime.year() < 2024 || rtcTime.year() > 2099)
  {
    logger.printf("WARNING: RTC time is invalid or out of range (year=%d). Cannot seed system clock.\n",
                  rtcTime.isValid() ? rtcTime.year() : 0);
    return;
  }

  // RTC stores UTC. Set the ESP32 system clock to this UTC epoch.
  struct timeval tv;
  tv.tv_sec = (time_t)rtcTime.unixtime();
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);

  logger.printf("System clock seeded from RTC: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                rtcTime.year(), rtcTime.month(), rtcTime.day(),
                rtcTime.hour(), rtcTime.minute(), rtcTime.second());
}

/**
 * @brief Periodically realigns the ESP32 system clock to the hardware RTC.
 *
 * The ESP32's internal crystal drifts significantly (seconds per hour).
 * This function compares the system clock against the DS3231 RTC (±2 ppm)
 * and corrects it if the drift exceeds 2 seconds. It is designed to be
 * called frequently (e.g., every 60 seconds) but skips work if an NTP
 * sync is actively running to avoid conflicts.
 */
void TimeManager::syncSystemClockFromRTC()
{
  // Don't fight with an active NTP sync — the SNTP daemon is updating
  // the system clock in the background.
  // Don't fight with an active NTP sync or the background SNTP daemon
  if (isNtpSyncInProgress())
  {
    return;
  }

  // If WiFi is connected, only defer to the SNTP daemon if it's currently in a successful state.
  // If the internet is down, SNTP will fail, and we MUST fall back to the RTC.
  if (WiFi.status() == WL_CONNECTED && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
  {
    return;
  }

  DateTime rtcTime = getRTCTime();
  if (!rtcTime.isValid() || rtcTime.year() < 2024 || rtcTime.year() > 2099)
  {
    return; // RTC time is invalid, cannot use as reference
  }

  // Compare RTC (UTC) against the system clock (UTC)
  time_t sysEpoch = time(nullptr);
  time_t rtcEpoch = (time_t)rtcTime.unixtime();
  int64_t drift = (int64_t)sysEpoch - (int64_t)rtcEpoch;

  if (abs(drift) >= 2)
  {
    struct timeval current_tv;
    gettimeofday(&current_tv, nullptr);
    struct timeval tv;
    tv.tv_sec = rtcEpoch;
    tv.tv_usec = current_tv.tv_usec;
    settimeofday(&tv, nullptr);

    SerialLog::getInstance().printf(
        "System clock realigned to RTC (drift was %+lds)\n", (long)drift);
  }
}

/**
 * @brief Initializes the TimeManager.
 *
 * Performs an initial, blocking NTP sync to set the time as soon as possible
 * after boot.
 */
void TimeManager::begin()
{
  // Note: RTC hardware initialization is handled externally in setupSensors()
  // to group all I2C device setups together.
  auto &logger = SerialLog::getInstance();

  // --- Step 1: Validate and seed system clock from hardware RTC ---
  // The ESP32's internal clock starts at epoch 0 on every boot.
  // Until SNTP succeeds, time(nullptr) returns garbage. Seed it
  // from the DS3231 so all timestamps are immediately meaningful,
  // even if NTP never connects.
  seedSystemClockFromRTC();

  // --- Step 2: Initialize the SNTP daemon (configures NTP servers and timezone) ---
  initNtp();

  // --- Step 3: Perform initial NTP sync ---
  logger.print("TimeManager: Performing initial NTP sync...\n");
  syncWithNTP();

  // --- Step 4: DST check ---
  // Right after the first sync we also run the DST check in case the
  // initial RTC value was already on a transition boundary.  This gives
  // added assurance that the internal DST flag is correct before the
  // main loop starts calling update().
  checkDST();
}

/**
 * @brief Main update loop for the TimeManager.
 *
 * This should be called repeatedly from the main application loop. It runs
 * approximately once per second and handles periodic tasks like checking for
 * missed alarms on the first run.
 *
 * @return True if a one-second tick has occurred, false otherwise.
 */
bool TimeManager::update()
{
  // Use a non-blocking delay to run the update logic frequently (50ms)
  // to catch the second transition accurately.
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate < UPDATE_INTERVAL)
  {
    return false; // No update occurred.
  }
  lastUpdate = currentMillis;

  DateTime now = getRTCTime();
  // Only trigger a full update if the second has actually changed, or if 1000ms has passed (fallback if RTC fails).
  static unsigned long lastSecondTick = 0;
  if (now.second() == _lastDecodedSecond && (currentMillis - lastSecondTick) < 1000)
  {
    return false;
  }
  _lastDecodedSecond = now.second();
  lastSecondTick = currentMillis;

  // Cache the time snapshot for consistent rendering within this frame (Local Time).
  // This prevents race conditions where the RTC may return a different
  // second when queried again during the render phase.
  {
    RecursiveLockGuard lock(_mutex);
    _cachedTime = getLocalTime(&now);
  }

#ifdef LOG_TICKS
  SerialLog::getInstance().print("TimeManager: Tick\n");
#endif
  // Check for DST transitions on every second tick using the pre-read RTC time (zero I2C overhead).
  checkDST(&now);

  // --- Layer 1: Periodic system clock realignment from RTC ---
  if (currentMillis - _lastSystemClockSync >= SYSTEM_CLOCK_SYNC_INTERVAL)
  {
    syncSystemClockFromRTC();
    _lastSystemClockSync = currentMillis;
  }

  if (!_rtc_alarms_initialized)
  {
    // On the first run, check if any alarms were missed while the device was off.
    checkMissedAlarms();
    // Then, set the hardware alarms for future events.
    setNextAlarms();
    _rtc_alarms_initialized = true;
  }

  // Update alarm cache if the minute has changed
  // 'localNow' is used for alarm calculations
  DateTime localNow = getCachedTime();
  if (localNow.minute() != _lastCacheUpdateMinute)
  {
    updateNextAlarmsCache(localNow);
  }

  return true; // An update occurred.
}

/**
 * @brief Triggers a non-blocking NTP sync.
 */
void TimeManager::syncWithNTP()
{
  SerialLog::getInstance().print("TimeManager: Starting background NTP sync...\n");
  startNtpSync();
}

/**
 * @brief Drives the non-blocking NTP synchronization state machine.
 *
 * This should be called in the main loop. It checks the status of the
 * ongoing NTP sync and, upon success, updates the last sync date.
 */
bool TimeManager::updateNtp()
{
  NtpSyncState state = updateNtpSync();
  if (state == NTP_SYNC_SUCCESS)
  {
    SerialLog::getInstance().print("TimeManager: NTP sync successful.\n");
    DateTime now = getLocalTime();
    {
      RecursiveLockGuard lock(_mutex);
      uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
      lastSyncDate = ymd;
      SerialLog::getInstance().printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
    }
    resetNtpSync();
    return true;
  }
  else if (state == NTP_SYNC_FAILED)
  {
    SerialLog::getInstance().print("TimeManager: NTP sync failed.\n");
    resetNtpSync();
  }
  return false;
}

/**
 * @brief Gets the current time formatted as a string.
 *
 * The format depends on the user's 12/24-hour preference.
 *
 * @return The formatted time string (e.g., "14:30" or "2:30").
 */
void TimeManager::getFormattedTime(char *buf, size_t bufSize) const
{
  getFormattedTime(buf, bufSize, getCachedTime());
}

void TimeManager::getFormattedTime(char *buf, size_t bufSize, const DateTime &now) const
{
  formatTime(buf, bufSize, now, is24HourFormat());
}

String TimeManager::getFormattedTime() const
{
  char buf[8];
  getFormattedTime(buf, sizeof(buf));
  return String(buf);
}

/**
 * @brief Gets the seconds part of the current time, zero-padded.
 * @return The formatted seconds string (e.g., "05").
 */
void TimeManager::getFormattedSeconds(char *buf, size_t bufSize) const
{
  getFormattedSeconds(buf, bufSize, getCachedTime());
}

void TimeManager::getFormattedSeconds(char *buf, size_t bufSize, const DateTime &now) const
{
  formatSeconds(buf, bufSize, now);
}

String TimeManager::getFormattedSeconds() const
{
  char buf[4];
  getFormattedSeconds(buf, sizeof(buf));
  return String(buf);
}

/**
 * @brief Gets the current date formatted as "MON DAY".
 * @return The formatted date string (e.g., "OCT 26").
 */
void TimeManager::getFormattedDate(char *buf, size_t bufSize) const
{
  getFormattedDate(buf, bufSize, getCachedTime());
}

void TimeManager::getFormattedDate(char *buf, size_t bufSize, const DateTime &now) const
{
  formatDate(buf, bufSize, now);
}

String TimeManager::getFormattedDate() const
{
  char buf[12];
  getFormattedDate(buf, sizeof(buf));
  return String(buf);
}

/**
 * @brief Gets the time-of-day indicator (AM/PM).
 * @return "AM", "PM", or an empty string if in 24-hour format.
 */
void TimeManager::getTOD(char *buf, size_t bufSize) const
{
  getTOD(buf, bufSize, getCachedTime());
}

void TimeManager::getTOD(char *buf, size_t bufSize, const DateTime &now) const
{
  formatTOD(buf, bufSize, now, is24HourFormat());
}

String TimeManager::getTOD() const
{
  char buf[4];
  getTOD(buf, sizeof(buf));
  return String(buf);
}

/**
 * @brief Gets the day of the week as a three-letter abbreviation.
 * @return The abbreviated day of the week (e.g., "SUN").
 */
void TimeManager::getDayOfWeek(char *buf, size_t bufSize) const
{
  getDayOfWeek(buf, bufSize, getCachedTime());
}

void TimeManager::getDayOfWeek(char *buf, size_t bufSize, const DateTime &now) const
{
  formatDayOfWeek(buf, bufSize, now);
}

String TimeManager::getDayOfWeek() const
{
  char buf[4];
  getDayOfWeek(buf, sizeof(buf));
  return String(buf);
}

/**
 * @brief Checks if the clock is set to 24-hour format.
 * @return True if 24-hour format is enabled, false for 12-hour.
 */
bool TimeManager::is24HourFormat() const
{
  // Delegate the check to the ConfigManager to centralize settings access.
  return ConfigManager::getInstance().is24HourFormat();
}

/**
 * @brief Gets the current hour in 24-hour format.
 * @return The hour (0-23).
 */
uint8_t TimeManager::getHour() const
{
  // Return the local hour (0-23).
  DateTime now = getLocalTime();
  return now.hour();
}

/**
 * @brief Checks if a daily NTP sync is required.
 *
 * This function is designed to run once a day (e.g., at 3 AM) to correct
 * any clock drift. It compares the current date with the last sync date.
 */
void TimeManager::checkDailySync()
{
  DateTime now = getLocalTime();
  if (now.hour() < 2)
  {
    return;
  }

  // Create a YYYYMMDD integer for the current date.
  uint32_t today = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();

  bool needsSync = false;
  {
    RecursiveLockGuard lock(_mutex);
    // If the last sync was on a different day, trigger a sync.
    if (lastSyncDate != today)
    {
      // Stamp lastSyncDate immediately to prevent this from firing again
      // on every logicTask iteration (every 10ms) for the rest of the day.
      // If the sync fails, checkDriftAndResync() will catch residual drift.
      lastSyncDate = today;
      needsSync = true;
    }
  }

  if (needsSync)
  {
    SerialLog::getInstance().print("Performing daily time sync...\n");
    startNtpSync(); // This starts the non-blocking NTP sync.
  }
}

/**
 * @brief Periodically checks the RTC's drift against an NTP server.
 *
 * This function runs at a set interval (e.g., every 4 hours) to compare the
 * local RTC time with the accurate time from an NTP server. If the drift
 * exceeds a predefined threshold, it triggers a non-blocking resynchronization.
 */
void TimeManager::checkDriftAndResync()
{
  // How often to check for drift (e.g., every 4 hours).
  static const unsigned long DRIFT_CHECK_INTERVAL = 4UL * 60 * 60 * 1000;
  // First check runs early (5 minutes) to catch post-crash drift quickly.
  static const unsigned long INITIAL_DRIFT_CHECK_DELAY = 5UL * 60 * 1000;

  unsigned long currentMillis = millis();
  unsigned long interval = _initialDriftCheckDone ? DRIFT_CHECK_INTERVAL : INITIAL_DRIFT_CHECK_DELAY;
  if (currentMillis - lastDriftCheck < interval)
  {
    return; // Not time for a drift check yet.
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    return; // Can't check drift without WiFi
  }

  lastDriftCheck = currentMillis;
  _initialDriftCheckDone = true;

  // Trigger a non-blocking NTP sync. The sync captures pre-sync snapshots
  // of all three time sources (Layer 3) and, upon success, logs a full
  // three-source diagnostic dashboard and corrects the RTC if needed.
  SerialLog::getInstance().print("Performing periodic drift check via NTP sync...\n");
  startNtpSync();
}

void TimeManager::checkDST(const DateTime *preReadUtc)
{
  // This method is invoked on every second transition in update(), once during begin(),
  // and immediately when timezone is modified.
  DateTime utc = (preReadUtc != nullptr) ? *preReadUtc : getRTCTime();
  if (!utc.isValid())
  {
    return;
  }

  // With UTC in the RTC, we can get the epoch directly from unixtime().
  // localtime_r is then authoritative for determining the correct DST state.
  time_t epoch = (time_t)utc.unixtime();
  struct tm resolved;
  {
    RecursiveLockGuard lock(_mutex);
    if (localtime_r(&epoch, &resolved) == nullptr) {
        return; // Epoch was invalid
    }
  }

  if (resolved.tm_isdst < 0)
  {
    return; // DST information unavailable
  }

  bool currentDstState = ConfigManager::getInstance().isDST();
  bool newDstState = resolved.tm_isdst > 0;

  if (newDstState != currentDstState)
  {
    ConfigManager::getInstance().setDST(newDstState);
    SerialLog::getInstance().printf("DST State updated: %d -> %d\n", currentDstState, newDstState);

    // If transitioning into DST (Spring Forward), local time jumped forward by 1 hour.
    // Check for any alarms that were scheduled in the skipped hour window.
    if (newDstState && !currentDstState)
    {
      checkMissedAlarmsWindow(epoch - 3600, epoch);
    }

    // Reschedule hardware RTC alarms immediately to reflect the new UTC offset
    setNextAlarms();
  }
}

DateTime TimeManager::getLocalTime(const DateTime *preReadUtc) const
{
  DateTime utc = (preReadUtc != nullptr) ? *preReadUtc : getRTCTime();
  if (!utc.isValid())
  {
    return utc;
  }

  // DateTime::unixtime() returns seconds since 1970-01-01 00:00:00 UTC.
  // Since the RTC stores UTC, this gives us the exact epoch without any
  // manual struct-to-time_t conversion (and avoids the non-standard timegm).
  time_t now_utc = (time_t)utc.unixtime();

  struct tm t_local;
  {
    RecursiveLockGuard lock(_mutex);
    if (localtime_r(&now_utc, &t_local) == nullptr) {
        return utc; // Fallback
    }
  }

  return DateTime(t_local.tm_year + 1900, t_local.tm_mon + 1, t_local.tm_mday,
                  t_local.tm_hour, t_local.tm_min, t_local.tm_sec);
}

void TimeManager::updateSnoozeStates()
{
  if (AlarmManager::getInstance().isRinging())
  {
    return;
  }
  auto &config = ConfigManager::getInstance();
  std::vector<Alarm> alarms = config.getAllAlarms();

  for (auto &alarm : alarms)
  {
    if (alarm.isEnabled() && alarm.isSnoozed())
    {
      if (alarm.updateSnooze())
      {
        // Snooze is over, re-trigger the alarm.
        // If it succeeds, clear the snooze state. If it fails (e.g., another
        // alarm is already ringing), leave it snoozed so it retries later.
        if (AlarmManager::getInstance().trigger(alarm.getId()))
        {
          // Atomically read-modify-write the alarm to preserve concurrent changes
          Alarm freshAlarm = config.getAlarmById(alarm.getId());
          if (freshAlarm.getId() != 255)
          {
            freshAlarm.setSnoozeState(false, 0);
            config.setAlarmById(alarm.getId(), freshAlarm);
          }
        }
        break; // Only trigger one alarm at a time
      }
    }
  }
}

DateTime TimeManager::getRTCTime() const
{
  // Guard against calling RTC.now() before the I2C bus and RTC hardware are
  // initialized. Without this, an early call (e.g. from AlarmManager::update
  // before setupSensors() completes) talks to uninitialized hardware.
  if (!isRtcFound())
  {
    return DateTime(); // Returns an invalid DateTime (year < 2000)
  }

  // --- Layer 2: Double-read I2C validation ---
  // Read the RTC twice in quick succession. If the I2C bus is corrupted,
  // the two readings will differ wildly. Valid readings should agree
  // within 1 second (the read takes ~1ms, so at most one second boundary
  // can be crossed between reads).
  DateTime read1;
  DateTime read2;
  {
    RecursiveLockGuard i2cLock(I2CBus::getMutex());
    read1 = RTC.now();
    read2 = RTC.now();
  }

  RecursiveLockGuard lock(_mutex);

  if (!read1.isValid() || !read2.isValid())
  {
    SerialLog::getInstance().print("WARNING: RTC read returned invalid DateTime\n");
    // Return last known-good if available
    if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
    {
      return _lastValidRtcTime;
    }
    return DateTime(); // No fallback available
  }

  int64_t readDelta = abs((int64_t)read1.unixtime() - (int64_t)read2.unixtime());
  if (readDelta > 1)
  {
    SerialLog::getInstance().printf(
        "WARNING: RTC double-read mismatch! read1=%lu, read2=%lu (delta=%lld). Using last known-good.\n",
        (unsigned long)read1.unixtime(), (unsigned long)read2.unixtime(), (long long)readDelta);
    if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
    {
      return _lastValidRtcTime;
    }
    return read2; // No fallback, use the more recent read
  }

  // Sanity check: year must be in valid range
  if (read2.year() < 2024 || read2.year() > 2099)
  {
    SerialLog::getInstance().printf(
        "WARNING: RTC time out of range (year=%d). Using last known-good.\n", read2.year());
    if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
    {
      return _lastValidRtcTime;
    }
    return DateTime();
  }

  // Monotonicity check: time should not jump by more than 5 seconds.
  // A small backward jump (1-2s) can happen legitimately due to NTP corrections.
  if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
  {
    int64_t timeDelta = (int64_t)read2.unixtime() - (int64_t)_lastValidRtcTime.unixtime();
    static int consecutiveJumps = 0;
    if (abs((long long)timeDelta) > 5)
    {
      consecutiveJumps++;
      if (consecutiveJumps < 3)
      {
        SerialLog::getInstance().printf(
            "WARNING: RTC time jumped by %lld seconds. Rejecting.\n",
            (long long)timeDelta);
        return _lastValidRtcTime;
      }
      SerialLog::getInstance().printf("WARNING: RTC time jumped by %lld seconds. Accepting after consecutive reads.\n", (long long)timeDelta);
      consecutiveJumps = 0;
    }
    else
    {
      consecutiveJumps = 0;
    }
  }

  // All validations passed — update the last known-good value
  _lastValidRtcTime = read2;
  return read2;
}

DateTime TimeManager::getCachedTime() const
{
  RecursiveLockGuard lock(_mutex);
  // If no time has been cached yet (before first update), return local time
  // so callers always get a locally-adjusted value.
  if (!_cachedTime.isValid() || _cachedTime.year() < 2000)
  {
    return getLocalTime();
  }
  return _cachedTime;
}

void TimeManager::adjustRTC(const DateTime &newTime)
{
  {
    RecursiveLockGuard i2cLock(I2CBus::getMutex());
    RTC.adjust(newTime);
  }
  RecursiveLockGuard lock(_mutex);
  // Update the last valid time to prevent the monotonicity check in
  // getRTCTime() from rejecting the new (potentially backward-adjusted) time.
  _lastValidRtcTime = newTime;
}

bool TimeManager::isTimeSet() const
{
  // Guard against calling RTC.lostPower() before the hardware is initialized,
  // matching the same guard used in getRTCTime().
  if (!isRtcFound())
  {
    return false;
  }
  bool lostPower = true;
  {
    RecursiveLockGuard i2cLock(I2CBus::getMutex());
    lostPower = RTC.lostPower();
  }
  return !lostPower;
}

void TimeManager::checkMissedAlarms()
{
  time_t nowEpoch = (time_t)getRTCTime().unixtime();
  checkMissedAlarmsWindow(nowEpoch - 90 * 60, nowEpoch);
}

void TimeManager::checkMissedAlarmsWindow(time_t startEpoch, time_t endEpoch)
{
  if (AlarmManager::getInstance().isRinging() || AlarmManager::getInstance().isResumePending())
  {
    return;
  }

  if (endEpoch - startEpoch > 90 * 60)
  {
    startEpoch = endEpoch - 90 * 60;
  }
  if (startEpoch >= endEpoch) return;

  SerialLog::getInstance().printf("Checking for missed alarms from %lu to %lu...\n", (unsigned long)startEpoch, (unsigned long)endEpoch);

  int8_t earliestMissedAlarmId = -1;
  auto &config = ConfigManager::getInstance();
  std::vector<Alarm> alarms = config.getAllAlarms();

  struct tm prev_t_local = {0};
  bool first = true;

  for (time_t t = startEpoch; t <= endEpoch; t += 60)
  {
    struct tm t_local;
    {
      RecursiveLockGuard lock(_mutex);
      if (localtime_r(&t, &t_local) == nullptr) {
          continue; // Skip invalid time
      }
    }
    DateTime checkLocal(
        t_local.tm_year + 1900, t_local.tm_mon + 1, t_local.tm_mday,
        t_local.tm_hour, t_local.tm_min, t_local.tm_sec);

    // Detect skipped hour due to DST spring forward
    if (!first && t_local.tm_hour != prev_t_local.tm_hour)
    {
      int expected_hour = (prev_t_local.tm_hour + 1) % 24;
      if (t_local.tm_hour != expected_hour)
      {
          int skipped_hour = expected_hour;
          while (skipped_hour != t_local.tm_hour)
          {
            int day_offset = (skipped_hour == 0) ? 1 : 0;
            DateTime baseDate(prev_t_local.tm_year + 1900, prev_t_local.tm_mon + 1, prev_t_local.tm_mday, 0, 0, 0);
            if (day_offset != 0)
            {
              baseDate = baseDate + TimeSpan(1, 0, 0, 0);
            }

            for (int m = 0; m < 60; m++)
            {
              DateTime checkSkipped(
                baseDate.year(), baseDate.month(), baseDate.day(),
                skipped_hour, m, 0);

              for (const auto &alarm : alarms)
              {
                if (alarm.isEnabled() && !alarm.isSnoozed() && alarm.shouldRing(checkSkipped))
                {
                  if (earliestMissedAlarmId == -1) earliestMissedAlarmId = alarm.getId();
                }
              }
            }
            skipped_hour = (skipped_hour + 1) % 24;
          }
      }
    }

    for (const auto &alarm : alarms)
    {
      if (alarm.isEnabled() && !alarm.isSnoozed() && alarm.shouldRing(checkLocal))
      {
        if (earliestMissedAlarmId == -1)
        {
          earliestMissedAlarmId = alarm.getId();
        }
      }
    }
    if (earliestMissedAlarmId != -1)
    {
      break;
    }
    prev_t_local = t_local;
    first = false;
  }

  if (earliestMissedAlarmId != -1)
  {
    SerialLog::getInstance().printf("Found missed alarm %d. Triggering now.\n", earliestMissedAlarmId);
    if (!AlarmManager::getInstance().trigger(earliestMissedAlarmId))
    {
       dismissAlarm(earliestMissedAlarmId);
    }
  }
  else
  {
    SerialLog::getInstance().print("No missed alarms found.\n");
  }
}

void TimeManager::dismissAlarm(uint8_t alarmId)
{
  auto &config = ConfigManager::getInstance();
  Alarm alarm = config.getAlarmById(alarmId);
  if (alarm.getId() != 255)
  {
    if (alarm.getDays() == 0 && !alarm.isSnoozed())
    {
      alarm.setEnabled(false);
      config.setAlarmById(alarmId, alarm);
    }
    else if (alarm.getDays() != 0 && !alarm.isSnoozed())
    {
      alarm.dismiss(getLocalTime());
      config.setAlarmById(alarmId, alarm);
    }
  }
}

void TimeManager::handleAlarm()
{
  setNextAlarms();
}

void TimeManager::clearRtcAlarms()
{
  RecursiveLockGuard i2cLock(I2CBus::getMutex());
  RTC.clearAlarm(1);
  RTC.clearAlarm(2);
  RTC.disableAlarm(1);
  RTC.disableAlarm(2);
  RTC.writeSqwPinMode(DS3231_OFF);
}

void TimeManager::updateNextAlarmsCache()
{
  updateNextAlarmsCache(getLocalTime());
}

void TimeManager::updateNextAlarmsCache(const DateTime &now)
{
  auto &config = ConfigManager::getInstance();
  std::vector<NextAlarmTime> nextAlarms;

  {
    RecursiveLockGuard lock(_mutex);
    _lastCacheUpdateMinute = now.minute();
  }

  std::vector<Alarm> alarms = config.getAllAlarms();
  for (const auto &alarm : alarms)
  {
    if (alarm.isEnabled())
    {
      DateTime next = calculateNextRingTime(alarm, now);
      if (next.isValid())
      {
        nextAlarms.push_back({next, alarm.getId()});
      }
    }
  }

  std::sort(nextAlarms.begin(), nextAlarms.end());

  RecursiveLockGuard lock(_mutex);
  _cachedNextAlarms = nextAlarms;
}

std::vector<NextAlarmTime> TimeManager::getNextAlarms(int count) const
{
  RecursiveLockGuard lock(_mutex);
  std::vector<NextAlarmTime> result = _cachedNextAlarms;
  if (result.size() > count)
  {
    result.resize(count);
  }
  return result;
}

void TimeManager::setNextAlarms()
{
  int8_t firedAlarm1 = -1;
  int8_t firedAlarm2 = -1;

  {
    RecursiveLockGuard i2cLock(I2CBus::getMutex());
    if (RTC.alarmFired(1))
    {
      RTC.clearAlarm(1);
      RecursiveLockGuard lock(_mutex);
      firedAlarm1 = _rtcAlarm1Id;
    }
    if (RTC.alarmFired(2))
    {
      RTC.clearAlarm(2);
      RecursiveLockGuard lock(_mutex);
      firedAlarm2 = _rtcAlarm2Id;
    }
  }

  uint32_t currentEpoch = time(nullptr);
  if (currentEpoch < 100000)
  {
    currentEpoch = getRTCTime().unixtime();
  }

  if (firedAlarm1 != -1)
  {
    SerialLog::getInstance().printf("RTC alarm 1 fired for alarm ID %d\n", firedAlarm1);
    AlarmManager::getInstance().trigger(firedAlarm1, currentEpoch);
  }
  if (firedAlarm2 != -1)
  {
    SerialLog::getInstance().printf("RTC alarm 2 fired for alarm ID %d\n", firedAlarm2);
    AlarmManager::getInstance().trigger(firedAlarm2, currentEpoch);
  }

  clearRtcAlarms();

  // Update cache first to ensure we have latest (local times)
  updateNextAlarmsCache();

  // We need at least 2 alarms for RTC setting
  std::vector<NextAlarmTime> nextAlarms = getNextAlarms(2);

  {
    RecursiveLockGuard lock(_mutex);
    _rtcAlarm1Id = !nextAlarms.empty() ? nextAlarms[0].id : -1;
    _rtcAlarm2Id = (nextAlarms.size() > 1) ? nextAlarms[1].id : -1;
  }

  if (!nextAlarms.empty())
  {
    // Store local ring time converted back to UTC for the hardware alarm
    DateTime utcTime = localToUtc(nextAlarms[0].time);
    if (utcTime.isValid())
    {
      {
        RecursiveLockGuard i2cLock(I2CBus::getMutex());
        RTC.setAlarm1(utcTime, DS3231_A1_Date);
      }
      
      char buf[60];
      snprintf(buf, sizeof(buf), "Set RTC alarm 1 for %04d-%02d-%02d %02d:%02d:%02d (LOCAL)\n",
               nextAlarms[0].time.year(), nextAlarms[0].time.month(), nextAlarms[0].time.day(),
               nextAlarms[0].time.hour(), nextAlarms[0].time.minute(), nextAlarms[0].time.second());
      SerialLog::getInstance().print(buf);
    }
  }

  if (nextAlarms.size() > 1)
  {
    // Store local ring time converted back to UTC for the hardware alarm
    DateTime utcTime = localToUtc(nextAlarms[1].time);
    if (utcTime.isValid())
    {
      {
        RecursiveLockGuard i2cLock(I2CBus::getMutex());
        RTC.setAlarm2(utcTime, DS3231_A2_Date);
      }
      
      char buf[60];
      snprintf(buf, sizeof(buf), "Set RTC alarm 2 for %04d-%02d-%02d %02d:%02d (LOCAL)\n",
               nextAlarms[1].time.year(), nextAlarms[1].time.month(), nextAlarms[1].time.day(),
               nextAlarms[1].time.hour(), nextAlarms[1].time.minute());
      SerialLog::getInstance().print(buf);
    }
  }
}

DateTime TimeManager::localToUtc(const DateTime &local) const
{
  if (!local.isValid()) return local;

  struct tm t_local = {};
  t_local.tm_year = local.year() - 1900;
  t_local.tm_mon = local.month() - 1;
  t_local.tm_mday = local.day();
  t_local.tm_hour = local.hour();
  t_local.tm_min = local.minute();
  t_local.tm_sec = local.second();
  t_local.tm_isdst = -1; // Let system determine

  time_t now_utc;
  {
    RecursiveLockGuard lock(_mutex);
    now_utc = mktime(&t_local);
  }

  if (now_utc == (time_t)(-1))
  {
    return DateTime(); // Conversion failed
  }

  struct tm t_utc;
  gmtime_r(&now_utc, &t_utc);

  return DateTime(t_utc.tm_year + 1900, t_utc.tm_mon + 1, t_utc.tm_mday,
                  t_utc.tm_hour, t_utc.tm_min, t_utc.tm_sec);
}
