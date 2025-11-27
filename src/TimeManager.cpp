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

#include <vector>
#include <algorithm>

// Structure to hold an alarm's next occurrence time and its ID
struct NextAlarmTime
{
  DateTime time;
  uint8_t id;

  bool operator<(const NextAlarmTime &other) const
  {
    return time < other.time;
  }
};

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
      return DateTime(now.year(), now.month(), now.day(), alarm.getHour(), alarm.getMinute(), 0);
    }
  }

  // Check for the next 7 days
  for (int i = 1; i <= 7; ++i)
  {
    uint8_t dayOfWeek = (now.dayOfTheWeek() + i) % 7;
    if (alarm.getDays() == 0 || (alarm.getDays() & (1 << dayOfWeek)))
    {
      DateTime next = now + TimeSpan(i, 0, 0, 0);
      return DateTime(next.year(), next.month(), next.day(), alarm.getHour(), alarm.getMinute(), 0);
    }
  }

  return DateTime(); // No valid ring time found
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
  // Perform an initial NTP sync attempt at startup.
  SerialLog::getInstance().print("TimeManager: Performing initial NTP sync...\n");
  syncWithNTP();
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
  // Use a non-blocking delay to run the update logic approximately once per second.
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate < UPDATE_INTERVAL)
  {
    return false; // No update occurred.
  }
  lastUpdate = currentMillis;

#ifdef LOG_TICKS
  SerialLog::getInstance().print("TimeManager: Tick\n");
#endif
  // Perform routine checks, like the daily time sync.
  checkDST();

  if (!_rtc_alarms_initialized)
  {
    // On the first run, check if any alarms were missed while the device was off.
    checkMissedAlarms();
    // Then, set the hardware alarms for future events.
    setNextAlarms();
    _rtc_alarms_initialized = true;
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
    DateTime now = RTC.now();
    // Store the date as a single integer (e.g., 20231026) for easy comparison.
    uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day(); // -1 from day to test immediately re-sync after initial sync
    lastSyncDate = ymd;
    SerialLog::getInstance().printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
  }
}

/**
 * @brief Drives the non-blocking NTP synchronization state machine.
 *
 * This should be called in the main loop. It checks the status of the
 * ongoing NTP sync and, upon success, updates the last sync date.
 */
void TimeManager::updateNtp()
{
  NtpSyncState state = updateNtpSync();
  if (state == NTP_SYNC_SUCCESS)
  {
    SerialLog::getInstance().print("TimeManager: NTP sync successful.\n");
    DateTime now = RTC.now();
    uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    lastSyncDate = ymd;
    SerialLog::getInstance().printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
    resetNtpSync();
  }
  else if (state == NTP_SYNC_FAILED)
  {
    SerialLog::getInstance().print("TimeManager: NTP sync failed.\n");
    resetNtpSync();
  }
}

/**
 * @brief Gets the current time formatted as a string.
 *
 * The format depends on the user's 12/24-hour preference.
 *
 * @return The formatted time string (e.g., "14:30" or "2:30").
 */
String TimeManager::getFormattedTime() const
{
  DateTime now = RTC.now();
  char timeStr[6]; // Buffer for "HH:MM"

  // Format the time based on the user's preference (12/24 hour).
  if (is24HourFormat())
  {
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
  }
  else
  {
    // Convert 24-hour time to 12-hour format.
    int hour12 = now.hour() % 12;
    if (hour12 == 0)
      hour12 = 12; // 0 o'clock is 12 AM.
    sprintf(timeStr, "%d:%02d", hour12, now.minute());
  }
  return String(timeStr);
}

/**
 * @brief Gets the seconds part of the current time, zero-padded.
 * @return The formatted seconds string (e.g., "05").
 */
String TimeManager::getFormattedSeconds() const
{
  DateTime now = RTC.now();
  char secondsStr[3];
  sprintf(secondsStr, "%02d", now.second());
  return String(secondsStr);
}

/**
 * @brief Gets the current date formatted as "MON DAY".
 * @return The formatted date string (e.g., "OCT 26").
 */
String TimeManager::getFormattedDate() const
{
  DateTime now = RTC.now();
  // Array of month abbreviations for a compact date format.
  static const char *monthNames[] = {
      "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
      "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char dateStr[12];
  // Format as "MON DAY" (e.g., "OCT 26").
  snprintf(dateStr, sizeof(dateStr), "%s %d", monthNames[now.month() - 1], now.day());
  return String(dateStr);
}

/**
 * @brief Gets the time-of-day indicator (AM/PM).
 * @return "AM", "PM", or an empty string if in 24-hour format.
 */
String TimeManager::getTOD() const
{
  // Return an empty string if in 24-hour mode, as AM/PM is not needed.
  if (is24HourFormat())
  {
    return "";
  }
  DateTime now = RTC.now();
  // Determine AM or PM based on the hour.
  return (now.hour() < 12) ? "AM" : "PM";
}

/**
 * @brief Gets the day of the week as a three-letter abbreviation.
 * @return The abbreviated day of the week (e.g., "SUN").
 */
String TimeManager::getDayOfWeek() const
{
  DateTime now = RTC.now();
  // Array of day abbreviations.
  static const char *dayNames[] = {
      "SUN", "MON", "TUE", "WED",
      "THU", "FRI", "SAT"};
  // The dayOfTheWeek() method returns an index (0=Sun, 1=Mon, etc.).
  return String(dayNames[now.dayOfTheWeek()]);
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
  // Return the raw hour (0-23) from the RTC.
  DateTime now = RTC.now();
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
  DateTime now = RTC.now();
  if (now.hour() < 2)
  {
    return;
  }

  // Create a YYYYMMDD integer for the current date.
  uint32_t today = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();

  // If the last sync was on a different day, and it's 3 AM or later, we should sync.
  // If the device was off at 3 AM
  // but is on at 4 AM, this will correctly trigger a sync.
  if (lastSyncDate < today)
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
  const unsigned long DRIFT_CHECK_INTERVAL = 4 * 60 * 60 * 1000;
  // The maximum acceptable drift in seconds before forcing a resync.
  const int DRIFT_THRESHOLD_SECONDS = 2;

  unsigned long currentMillis = millis();
  if (currentMillis - lastDriftCheck < DRIFT_CHECK_INTERVAL)
  {
    return; // Not time for a drift check yet.
  }
  lastDriftCheck = currentMillis;

  SerialLog::getInstance().print("Performing periodic clock drift check...\n");

  // Get the current time from the NTP server without adjusting the RTC.
  DateTime ntpTime = getNtpTime();
  if (!ntpTime.isValid())
  {
    SerialLog::getInstance().print("Drift check failed: Could not get NTP time.\n");
    return;
  }

  // Get the current time from the local RTC.
  DateTime rtcTime = RTC.now();
  // Calculate the time difference.
  TimeSpan drift = rtcTime - ntpTime;

  SerialLog::getInstance().printf("RTC vs NTP drift is %ld seconds.\n", drift.totalseconds());

  // If the absolute drift exceeds the threshold, start a non-blocking sync.
  if (abs(drift.totalseconds()) > DRIFT_THRESHOLD_SECONDS)
  {
    SerialLog::getInstance().print("Drift exceeds threshold. Triggering NTP resync...\n");
    startNtpSync();
  }
}

void TimeManager::checkDST()
{
  // Get current DST state from config
  bool currentDstState = ConfigManager::getInstance().isDST();

  DateTime now = RTC.now();
  struct tm t;
  t.tm_year = now.year() - 1900;
  t.tm_mon = now.month() - 1;
  t.tm_mday = now.day();
  t.tm_hour = now.hour();
  t.tm_min = now.minute();
  t.tm_sec = now.second();

  // Force mktime to use the LAST KNOWN DST state.
  // This allows it to detect invalid times (Spring Forward gap)
  // or ambiguous times (Fall Back overlap) correctly based on where we came from.
  t.tm_isdst = currentDstState ? 1 : 0;

  mktime(&t); // This normalizes 't' and updates tm_isdst

  bool newDstState = t.tm_isdst > 0;

  if (newDstState != currentDstState)
  {
    SerialLog::getInstance().printf("DST Transition Detected: %d -> %d\n", currentDstState, newDstState);
    ConfigManager::getInstance().setDST(newDstState);

    // Check if the time has shifted (which it should have)
    // We compare the normalized 't' with the original 'now'.
    // Note: t contains the corrected time.
    DateTime correctedTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

    if (correctedTime.hour() != now.hour() || correctedTime.minute() != now.minute())
    {
      SerialLog::getInstance().print("Adjusting RTC for DST...\n");
      RTC.adjust(correctedTime);
    }
  }
}

void TimeManager::updateSnoozeStates()
{
  if (AlarmManager::getInstance().isRinging())
  {
    return;
  }
  auto &config = ConfigManager::getInstance();
  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    Alarm &alarm = config.getAlarm(i);
    if (alarm.isEnabled() && alarm.isSnoozed())
    {
      if (alarm.updateSnooze())
      {
        // Snooze is over, re-trigger the alarm
        AlarmManager::getInstance().trigger(alarm.getId());
        // Persist the unsnoozed state
        config.setAlarm(i, alarm);
        config.save();
        break; // Only trigger one alarm at a time
      }
    }
  }
}

DateTime TimeManager::getRTCTime() const
{
  return RTC.now();
}

bool TimeManager::isTimeSet() const
{
  // The DS3231 RTC has a Lost Power flag that is more reliable than checking
  // the year.
  return !RTC.lostPower();
}

void TimeManager::checkMissedAlarms()
{
  if (AlarmManager::getInstance().isRinging())
  {
    return;
  }

  SerialLog::getInstance().print("Checking for missed alarms on boot...\n");

  DateTime now = RTC.now();
  // Don't look back further than 30 minutes.
  const uint32_t lookbehindSeconds = 30 * 60;
  DateTime startTime = now - TimeSpan(lookbehindSeconds);

  int8_t mostRecentMissedAlarmId = -1;
  auto &config = ConfigManager::getInstance();

  // To find the most recent missed alarm, we iterate chronologically through
  // the look-behind window and check all alarms for each minute. The last
  // one we find will be the most recent.
  DateTime t = DateTime(startTime.unixtime());
  DateTime checkTime = DateTime(t.year(), t.month(), t.day(), t.hour(), t.minute());

  for (; checkTime <= now; checkTime = checkTime + TimeSpan(0, 0, 1, 0))
  {
    for (int i = 0; i < config.getNumAlarms(); ++i)
    {
      Alarm &alarm = config.getAlarm(i);

      if (alarm.isEnabled() && !alarm.isSnoozed() && alarm.shouldRing(checkTime))
      {
        // This alarm should have rung. We record it as the most recent candidate.
        // If we find another one later in the loop, it will overwrite this one.
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

  setNextAlarms();
}

void TimeManager::clearRtcAlarms()
{
  RTC.clearAlarm(1);
  RTC.clearAlarm(2);
  RTC.disableAlarm(1);
  RTC.disableAlarm(2);
  RTC.writeSqwPinMode(DS3231_OFF);
}

void TimeManager::setNextAlarms()
{
  clearRtcAlarms();

  auto &config = ConfigManager::getInstance();
  std::vector<NextAlarmTime> nextAlarms;
  DateTime now = RTC.now();

  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    Alarm &alarm = config.getAlarm(i);
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

  if (!nextAlarms.empty())
  {
    _rtcAlarm1Id = nextAlarms[0].id;
    RTC.setAlarm1(nextAlarms[0].time, DS3231_A1_Date);
    char buf[40];
    snprintf(buf, sizeof(buf), "Set RTC alarm 1 for %04d-%02d-%02d %02d:%02d:%02d\n",
             nextAlarms[0].time.year(), nextAlarms[0].time.month(), nextAlarms[0].time.day(),
             nextAlarms[0].time.hour(), nextAlarms[0].time.minute(), nextAlarms[0].time.second());
    SerialLog::getInstance().print(buf);
  }

  if (nextAlarms.size() > 1)
  {
    _rtcAlarm2Id = nextAlarms[1].id;
    RTC.setAlarm2(nextAlarms[1].time, DS3231_A2_Date);
    char buf[40];
    snprintf(buf, sizeof(buf), "Set RTC alarm 2 for %04d-%02d-%02d %02d:%02d\n",
             nextAlarms[1].time.year(), nextAlarms[1].time.month(), nextAlarms[1].time.day(),
             nextAlarms[1].time.hour(), nextAlarms[1].time.minute());
    SerialLog::getInstance().print(buf);
  }
}