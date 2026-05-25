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
    return DateTime(alarm.getSnoozeUntil());
  }

  // Check today
  if (alarm.getHour() > now.hour() || (alarm.getHour() == now.hour() && alarm.getMinute() > now.minute()))
  {
    if (alarm.getDays() == 0 || (alarm.getDays() & (1 << now.dayOfTheWeek())))
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
  if (!rtcTime.isValid() || rtcTime.year() < 2024 || rtcTime.year() > 2040)
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
  if (isNtpSyncInProgress())
  {
    return;
  }

  DateTime rtcTime = getRTCTime();
  if (!rtcTime.isValid() || rtcTime.year() < 2024 || rtcTime.year() > 2040)
  {
    return; // RTC time is invalid, cannot use as reference
  }

  // Compare RTC (UTC) against the system clock (UTC)
  time_t sysEpoch = time(nullptr);
  time_t rtcEpoch = (time_t)rtcTime.unixtime();
  int32_t drift = (int32_t)sysEpoch - (int32_t)rtcEpoch;

  if (abs(drift) >= 2)
  {
    struct timeval tv;
    tv.tv_sec = rtcEpoch;
    tv.tv_usec = 0;
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
  // Prevent the periodic timer from firing immediately in the first
  // update() call by pretending that we just ran.
  _lastDstCheck = millis();
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
  // Only trigger a full update if the second has actually changed.
  if (now.second() == _lastDecodedSecond)
  {
    return false;
  }
  _lastDecodedSecond = now.second();

  // Cache the time snapshot for consistent rendering within this frame (Local Time).
  // This prevents race conditions where the RTC may return a different
  // second when queried again during the render phase.
  {
    RecursiveLockGuard lock(_mutex);
    _cachedTime = getLocalTime();
  }

#ifdef LOG_TICKS
  SerialLog::getInstance().print("TimeManager: Tick\n");
#endif
  // Perform routine checks, like the daily time sync and DST change.
  if (currentMillis - _lastDstCheck >= DST_CHECK_INTERVAL)
  {
    checkDST();
    _lastDstCheck = currentMillis;
  }

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
 * @brief Performs a blocking NTP sync and updates the last sync date.
 */
void TimeManager::syncWithNTP()
{
  // This will be the initial, blocking sync
  if (syncTime())
  {
    // If the sync was successful, update the last sync date.
    DateTime now = getRTCTime();
    RecursiveLockGuard lock(_mutex);
    // Store the date as a single integer (e.g., 20231026) for easy comparison.
    uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    lastSyncDate = ymd;
    SerialLog::getInstance().printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
  }
  else
  {
    // The blocking sync failed (SNTP daemon may not have a response yet).
    // Start the non-blocking sync state machine so the logicTask keeps
    // retrying via updateNtp() until we get a valid time.
    SerialLog::getInstance().print("Initial NTP sync failed. Starting non-blocking retry...\n");
    startNtpSync();
  }
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
    DateTime now = getRTCTime();
    RecursiveLockGuard lock(_mutex);
    uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    lastSyncDate = ymd;
    SerialLog::getInstance().printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
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
  DateTime now = getCachedTime();
  if (is24HourFormat())
  {
    snprintf(buf, bufSize, "%02d:%02d", now.hour(), now.minute());
  }
  else
  {
    int hour12 = now.hour() % 12;
    if (hour12 == 0)
      hour12 = 12;
    snprintf(buf, bufSize, "%d:%02d", hour12, now.minute());
  }
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
  DateTime now = getCachedTime();
  snprintf(buf, bufSize, "%02d", now.second());
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
  DateTime now = getCachedTime();
  static const char *monthNames[] = {
      "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  snprintf(buf, bufSize, "%s %d", monthNames[now.month() - 1], now.day());
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
  if (is24HourFormat())
  {
    buf[0] = '\0';
    return;
  }
  DateTime now = getCachedTime();
  snprintf(buf, bufSize, "%s", (now.hour() < 12) ? "AM" : "PM");
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
  DateTime now = getCachedTime();
  static const char *dayNames[] = {
      "SUN", "MON", "TUE", "WED",
      "THU", "FRI", "SAT"};
  snprintf(buf, bufSize, "%s", dayNames[now.dayOfTheWeek()]);
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
    if (lastSyncDate < today)
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

void TimeManager::checkDST()
{
  // This method is invoked periodically from update(), once during begin(),
  // and immediately when timezone is modified.
  bool currentDstState = ConfigManager::getInstance().isDST();

  DateTime utc = getRTCTime();
  if (!utc.isValid())
  {
    return;
  }

  // With UTC in the RTC, we can get the epoch directly from unixtime().
  // localtime_r is then authoritative for determining the correct DST state.
  time_t epoch = (time_t)utc.unixtime();
  struct tm resolved;
  localtime_r(&epoch, &resolved);

  bool newDstState = resolved.tm_isdst > 0;

  if (newDstState != currentDstState)
  {
    ConfigManager::getInstance().setDST(newDstState);
    SerialLog::getInstance().printf("DST State updated: %d -> %d\n", currentDstState, newDstState);
  }
}

DateTime TimeManager::getLocalTime() const
{
  DateTime utc = getRTCTime();
  if (!utc.isValid())
  {
    return utc;
  }

  // DateTime::unixtime() returns seconds since 1970-01-01 00:00:00 UTC.
  // Since the RTC stores UTC, this gives us the exact epoch without any
  // manual struct-to-time_t conversion (and avoids the non-standard timegm).
  time_t now_utc = (time_t)utc.unixtime();

  // localtime_r converts the UTC epoch to local time using the TZ variable.
  struct tm t_local;
  localtime_r(&now_utc, &t_local);

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
        // Snooze is over, re-trigger the alarm
        AlarmManager::getInstance().trigger(alarm.getId());
        // Persist the unsnoozed state by ID, as index might have changed
        config.setAlarmById(alarm.getId(), alarm);
        config.save();
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
  RecursiveLockGuard lock(_mutex);

  // --- Layer 2: Double-read I2C validation ---
  // Read the RTC twice in quick succession. If the I2C bus is corrupted,
  // the two readings will differ wildly. Valid readings should agree
  // within 1 second (the read takes ~1ms, so at most one second boundary
  // can be crossed between reads).
  DateTime read1 = RTC.now();
  DateTime read2 = RTC.now();

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

  int32_t readDelta = abs((int32_t)read1.unixtime() - (int32_t)read2.unixtime());
  if (readDelta > 1)
  {
    SerialLog::getInstance().printf(
        "WARNING: RTC double-read mismatch! read1=%lu, read2=%lu (delta=%ld). Using last known-good.\n",
        (unsigned long)read1.unixtime(), (unsigned long)read2.unixtime(), (long)readDelta);
    if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
    {
      return _lastValidRtcTime;
    }
    return read2; // No fallback, use the more recent read
  }

  // Sanity check: year must be in valid range
  if (read2.year() < 2024 || read2.year() > 2040)
  {
    SerialLog::getInstance().printf(
        "WARNING: RTC time out of range (year=%d). Using last known-good.\n", read2.year());
    if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
    {
      return _lastValidRtcTime;
    }
    return DateTime();
  }

  // Monotonicity check: time should not jump backward by more than 2 seconds.
  // A small backward jump (1-2s) can happen legitimately due to NTP corrections.
  if (_lastValidRtcTime.isValid() && _lastValidRtcTime.year() >= 2024)
  {
    int32_t timeDelta = (int32_t)read2.unixtime() - (int32_t)_lastValidRtcTime.unixtime();
    if (timeDelta < -2)
    {
      SerialLog::getInstance().printf(
          "WARNING: RTC time jumped backward by %ld seconds. Using last known-good.\n",
          (long)abs(timeDelta));
      return _lastValidRtcTime;
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

bool TimeManager::isTimeSet() const
{
  // Guard against calling RTC.lostPower() before the hardware is initialized,
  // matching the same guard used in getRTCTime().
  if (!isRtcFound())
  {
    return false;
  }
  RecursiveLockGuard lock(_mutex);
  return !RTC.lostPower();
}

void TimeManager::checkMissedAlarms()
{
  if (AlarmManager::getInstance().isRinging())
  {
    return;
  }

  SerialLog::getInstance().print("Checking for missed alarms on boot...\n");

  // Work in UTC epoch space since the RTC stores UTC.
  time_t nowEpoch = (time_t)getRTCTime().unixtime();
  const uint32_t lookbehindSeconds = 30 * 60;
  time_t startEpoch = nowEpoch - lookbehindSeconds;

  int8_t mostRecentMissedAlarmId = -1;
  auto &config = ConfigManager::getInstance();
  std::vector<Alarm> alarms = config.getAllAlarms();

  // Iterate minute-by-minute in UTC epoch space.
  // Convert each moment to local time before calling shouldRing(),
  // since alarm hour/minute are stored in local time.
  for (time_t t = startEpoch; t <= nowEpoch; t += 60)
  {
    struct tm t_local;
    localtime_r(&t, &t_local);
    DateTime checkLocal(
        t_local.tm_year + 1900, t_local.tm_mon + 1, t_local.tm_mday,
        t_local.tm_hour, t_local.tm_min, t_local.tm_sec);

    for (const auto &alarm : alarms)
    {
      if (alarm.isEnabled() && !alarm.isSnoozed() && alarm.shouldRing(checkLocal))
      {
        // Record the most recent candidate; overwrite as we find later ones.
        mostRecentMissedAlarmId = alarm.getId();
      }
    }
  }

  if (mostRecentMissedAlarmId != -1)
  {
    SerialLog::getInstance().printf("Found missed alarm %d. Triggering now.\n", mostRecentMissedAlarmId);
    AlarmManager::getInstance().trigger(mostRecentMissedAlarmId);
  }
  else
  {
    SerialLog::getInstance().print("No missed alarms found.\n");
  }
}

void TimeManager::handleAlarm()
{
  RecursiveLockGuard lock(_mutex);
  if (RTC.alarmFired(1))
  {
    RTC.clearAlarm(1);
    SerialLog::getInstance().printf("RTC alarm 1 fired for alarm ID %d\n", _rtcAlarm1Id);
    if (_rtcAlarm1Id != -1)
    {
      AlarmManager::getInstance().trigger(_rtcAlarm1Id);
    }
  }

  if (RTC.alarmFired(2))
  {
    RTC.clearAlarm(2);
    SerialLog::getInstance().printf("RTC alarm 2 fired for alarm ID %d\n", _rtcAlarm2Id);
    if (_rtcAlarm2Id != -1)
    {
      AlarmManager::getInstance().trigger(_rtcAlarm2Id);
    }
  }

  // Update hardware alarms
  setNextAlarms();
}

void TimeManager::clearRtcAlarms()
{
  // Assumes Mutex is held by caller if called internally
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
  RecursiveLockGuard lock(_mutex);
  clearRtcAlarms();

  // Update cache first to ensure we have latest (local times)
  updateNextAlarmsCache();

  // We need at least 2 alarms for RTC setting
  std::vector<NextAlarmTime> nextAlarms = getNextAlarms(2);

  if (!nextAlarms.empty())
  {
    _rtcAlarm1Id = nextAlarms[0].id;
    // Store local ring time converted back to UTC for the hardware alarm
    DateTime utcTime = localToUtc(nextAlarms[0].time);
    RTC.setAlarm1(utcTime, DS3231_A1_Date);
    
    char buf[60];
    snprintf(buf, sizeof(buf), "Set RTC alarm 1 for %04d-%02d-%02d %02d:%02d:%02d (LOCAL)\n",
             nextAlarms[0].time.year(), nextAlarms[0].time.month(), nextAlarms[0].time.day(),
             nextAlarms[0].time.hour(), nextAlarms[0].time.minute(), nextAlarms[0].time.second());
    SerialLog::getInstance().print(buf);
  }

  if (nextAlarms.size() > 1)
  {
    _rtcAlarm2Id = nextAlarms[1].id;
    // Store local ring time converted back to UTC for the hardware alarm
    DateTime utcTime = localToUtc(nextAlarms[1].time);
    RTC.setAlarm2(utcTime, DS3231_A2_Date);
    
    char buf[60];
    snprintf(buf, sizeof(buf), "Set RTC alarm 2 for %04d-%02d-%02d %02d:%02d (LOCAL)\n",
             nextAlarms[1].time.year(), nextAlarms[1].time.month(), nextAlarms[1].time.day(),
             nextAlarms[1].time.hour(), nextAlarms[1].time.minute());
    SerialLog::getInstance().print(buf);
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

  // mktime converts local tm to time_t (UTC epoch)
  time_t now_utc = mktime(&t_local);

  struct tm t_utc;
  gmtime_r(&now_utc, &t_utc);

  return DateTime(t_utc.tm_year + 1900, t_utc.tm_mon + 1, t_utc.tm_mday,
                  t_utc.tm_hour, t_utc.tm_min, t_utc.tm_sec);
}
