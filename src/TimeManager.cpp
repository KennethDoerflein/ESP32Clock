// TimeManager.cpp

#include "TimeManager.h"
#include "ntp.h"
#include "sensors.h"
#include "ConfigManager.h"
#include "AlarmManager.h"

void TimeManager::begin()
{
  // Note: RTC hardware initialization is handled externally in setupSensors()
  // to group all I2C device setups together.
  // Perform an initial NTP sync attempt at startup.
  syncWithNTP();
}

void TimeManager::update()
{
  // Use a non-blocking delay to run the update logic approximately once per second.
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate < UPDATE_INTERVAL)
  {
    return; // Not time to update yet
  }
  lastUpdate = currentMillis;

  // Perform routine checks, like the daily time sync.
  checkDailySync();
}

void TimeManager::syncWithNTP()
{
  // Call the global syncTime() function, which handles the NTP communication.
  if (syncTime())
  {
    // If the sync was successful, update the last sync date.
    DateTime now = RTC.now();
    // Store the date as a single integer (e.g., 20231026) for easy comparison.
    uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    lastSyncDate = ymd;
    Serial.printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
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
  // Trigger a daily sync at a specific time (e.g., 3 AM) when the network is likely quiet.
  if (now.hour() == 3)
  {
    // Create a YYYYMMDD integer for the current date.
    uint32_t today = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    // Only sync if the last sync was on a different day.
    if (lastSyncDate != today)
    {
      Serial.println("Performing daily 3 AM time sync...");
      syncWithNTP();
    }
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

  // Only check once per minute to avoid re-triggering.
  if (now.minute() == _lastMinuteChecked)
  {
    return;
  }
  _lastMinuteChecked = now.minute();

  auto &config = ConfigManager::getInstance();

  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    const Alarm &alarm = config.getAlarm(i);

    if (!alarm.enabled)
    {
      continue;
    }

    // Check if snooze time has passed.
    if (alarm.snoozed && millis() > alarm.snoozeUntil)
    {
      // Snooze is over, so we need to make a mutable copy to update it.
      Alarm mutableAlarm = alarm;
      mutableAlarm.snoozed = false;
      mutableAlarm.snoozeUntil = 0;
      config.setAlarm(i, mutableAlarm);
      // Continue to check if it should ring again now.
    }

    if (alarm.snoozed)
    {
      continue;
    }

    if (alarm.hour == now.hour() && alarm.minute == now.minute())
    {
      // Time matches. Now check day of the week.
      // RTClib dayOfTheWeek() is 0 for Sunday, 1 for Monday, etc.
      // Our bitmask is DAY_SUN = 1 << 0, DAY_MON = 1 << 1, etc.
      uint8_t todayBitmask = 1 << now.dayOfTheWeek();

      // Check if it's a repeating alarm for today, or a one-time alarm.
      if ((alarm.days & todayBitmask) > 0)
      {
        AlarmManager::getInstance().trigger(alarm);
        return; // Only trigger one alarm at a time.
      }
      else if (alarm.days == 0)
      {
        // This is a one-time alarm.
        AlarmManager::getInstance().trigger(alarm);
        // Disable it so it doesn't ring again tomorrow.
        Alarm mutableAlarm = alarm;
        mutableAlarm.enabled = false;
        config.setAlarm(i, mutableAlarm);
        return;
      }
    }
  }
}