#include "TimeManager.h"
#include "ntp.h"
#include "sensors.h"

void TimeManager::begin()
{
  // RTC initialization is handled in setupSensors()
  syncWithNTP();
}

void TimeManager::update()
{
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate < UPDATE_INTERVAL)
  {
    return;
  }
  lastUpdate = currentMillis;

  checkDailySync();
}

void TimeManager::syncWithNTP()
{
  syncTime(); // Using existing NTP sync function
  hasSyncedToday = true;
}

String TimeManager::getFormattedTime() const
{
  DateTime now = RTC.now();
  char timeStr[12];

  if (use24HourFormat)
  {
    sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  }
  else
  {
    int hour12 = now.hour() % 12;
    if (hour12 == 0)
      hour12 = 12;
    const char *ampm = now.hour() < 12 ? "AM" : "PM";
    sprintf(timeStr, "%2d:%02d:%02d %s", hour12, now.minute(), now.second(), ampm);
  }
  return String(timeStr);
}

String TimeManager::getFormattedDate() const
{
  DateTime now = RTC.now();
  static const char *monthNames[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char dateStr[20];
  sprintf(dateStr, "%s %d, %d", monthNames[now.month() - 1], now.day(), now.year());
  return String(dateStr);
}

String TimeManager::getDayOfWeek() const
{
  DateTime now = RTC.now();
  static const char *dayNames[] = {
      "SUN", "MON", "TUE", "WED",
      "THU", "FRI", "SAT"};
  return String(dayNames[now.dayOfTheWeek()]);
}

void TimeManager::checkDailySync()
{
  DateTime now = RTC.now();

  // Sync at 3 AM if not already synced today
  if (now.hour() == 3 && !hasSyncedToday)
  {
    syncWithNTP();
  }
  // Reset flag at 4 AM
  else if (now.hour() == 4)
  {
    hasSyncedToday = false;
  }
}