#include "TimeManager.h"
#include "ntp.h"
#include "sensors.h"

void TimeManager::begin()
{
  // RTC initialization is handled in setupSensors()
  // Initial sync attempt; only mark successful sync inside syncWithNTP
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
  // Call syncTime() which now returns bool indicating success
  if (syncTime())
  {
    DateTime now = RTC.now();
    // Compose YYYYMMDD integer using 32-bit arithmetic
    uint32_t ymd = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    lastSyncDate = ymd;
    Serial.printf("Marked lastSyncDate = %lu\n", (unsigned long)lastSyncDate);
  }
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
  // Sync at 3 AM if not already synced for this calendar date
  if (now.hour() == 3)
  {
    uint32_t today = (uint32_t)now.year() * 10000u + (uint32_t)now.month() * 100u + (uint32_t)now.day();
    if (lastSyncDate != today)
    {
      Serial.println("Performing daily 3 AM time sync...");
      syncWithNTP();
    }
  }
}