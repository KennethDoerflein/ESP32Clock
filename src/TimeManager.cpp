// TimeManager.cpp

// #define LOG_TICKS

#include "TimeManager.h"
#include "NtpSync.h"
#include "SensorModule.h"
#include "ConfigManager.h"
#include "AlarmManager.h"
#include "SerialLog.h"

#ifdef USE_RTC_ALARMS
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
#endif

void TimeManager::begin()
{
  // Note: RTC hardware initialization is handled externally in setupSensors()
  // to group all I2C device setups together.
  // Perform an initial NTP sync attempt at startup.
  SerialLog::getInstance().print("TimeManager: Performing initial NTP sync...\n");
  syncWithNTP();
}

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
#ifdef USE_RTC_ALARMS
  if (!_rtc_alarms_initialized)
  {
    // On the first run, check if any alarms were missed while the device was off.
    checkMissedAlarms();
    // Then, set the hardware alarms for future events.
    setNextAlarms();
    _rtc_alarms_initialized = true;
  }
#endif
  return true; // An update occurred.
}

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

String TimeManager::getFormattedSeconds() const
{
  DateTime now = RTC.now();
  char secondsStr[3];
  sprintf(secondsStr, "%02d", now.second());
  return String(secondsStr);
}

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

bool TimeManager::is24HourFormat() const
{
  // Delegate the check to the ConfigManager to centralize settings access.
  return ConfigManager::getInstance().is24HourFormat();
}

uint8_t TimeManager::getHour() const
{
  // Return the raw hour (0-23) from the RTC.
  DateTime now = RTC.now();
  return now.hour();
}

void TimeManager::checkDailySync()
{
  DateTime now = RTC.now();
  if (now.hour() < 3)
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

#ifndef USE_RTC_ALARMS
void TimeManager::checkAlarms()
{
  // If an alarm is already ringing, there's no need to check for others.
  if (AlarmManager::getInstance().isRinging())
  {
    return;
  }

  DateTime now = RTC.now();
  uint32_t nowSeconds = now.unixtime();

  // If this is the first run, we want to check for past alarms, but not
  // so far in the past that we trigger alarms from days ago. We'll start
  // our check from the current time, and the logic below will cap the
  // look-behind period at half an hour.
  uint32_t lastCheckSeconds = _lastTimeChecked;

  // If time has not advanced or went backwards, do nothing.
  if (nowSeconds <= lastCheckSeconds)
  {
    _lastTimeChecked = nowSeconds; // Keep track of potential time jumps
    return;
  }

  // To prevent a huge loop if the device was off, cap the catch-up time.
  // 0.5 hour should be more than enough to cover any reasonable NTP sync delay.
  const uint32_t maxCatchUpSeconds = 30 * 60;

  // On the first run after boot, _lastTimeChecked is 0.
  // We set our check start point to half an hour ago to find any alarms we missed.
  if (_lastTimeChecked == 0)
  {
    _lastTimeChecked = nowSeconds - maxCatchUpSeconds;
  }

  if (nowSeconds > lastCheckSeconds + maxCatchUpSeconds)
  {
    lastCheckSeconds = nowSeconds - maxCatchUpSeconds;
  }

  auto &config = ConfigManager::getInstance();
  bool alarmJustTriggered = false;

  // We iterate through each of the alarms first.
  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    Alarm &alarm = config.getAlarm(i);

    if (!alarm.isEnabled())
    {
      continue;
    }

    // Now, check every minute between the last check and the current time
    // to see if this alarm should have rung.
    // We start from the beginning of the *next* minute after the last check.
    DateTime t = DateTime(lastCheckSeconds);
    DateTime startTime = DateTime(t.year(), t.month(), t.day(), t.hour(), t.minute()) + TimeSpan(0, 0, 1, 0);

    for (DateTime checkMinute = startTime; checkMinute <= now; checkMinute = checkMinute + TimeSpan(0, 0, 1, 0))
    {
      if (alarm.shouldRing(checkMinute))
      {
        AlarmManager::getInstance().trigger(alarm.getId());
        alarmJustTriggered = true;

        // Note: One-time alarms are dismissed by the user (long press)
        // in main.cpp, not automatically. This allows them to be snoozed.
        break; // Stop checking minutes for this alarm
      }
    }

    if (alarmJustTriggered)
    {
      break; // Exit the main alarm loop
    }
  }

  // Always update the last checked time to the current time.
  _lastTimeChecked = nowSeconds;
}
#endif

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

#ifdef USE_RTC_ALARMS

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
#endif