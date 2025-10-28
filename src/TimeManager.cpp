// TimeManager.cpp

// #define LOG_TICKS

#include "TimeManager.h"
#include "ntp.h"
#include "sensors.h"
#include "ConfigManager.h"
#include "AlarmManager.h"
#include "SerialLog.h"

void TimeManager::begin()
{
  // Note: RTC hardware initialization is handled externally in setupSensors()
  // to group all I2C device setups together.
  SerialLog::getInstance().print("TimeManager: Initializing...\n");

  // If the RTC is already set, start a non-blocking sync in the background.
  // This prevents the UI from freezing on startup if the network is slow.
  if (isTimeSet())
  {
    SerialLog::getInstance().print("RTC time is valid. Starting non-blocking NTP sync.\n");
    startNtpSync();
  }
  else
  {
    // If the RTC is not set, perform a blocking sync to get the time immediately.
    SerialLog::getInstance().print("RTC time not set. Performing blocking NTP sync...\n");
    syncWithNTP();
  }
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