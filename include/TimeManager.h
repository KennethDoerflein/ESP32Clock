// TimeManager.h

#pragma once

#include <Arduino.h>
#include <RTClib.h>

class TimeManager
{
public:
  static TimeManager &getInstance()
  {
    static TimeManager instance;
    return instance;
  }

  void begin();
  void update();
  void syncWithNTP();
  String getFormattedTime() const;
  String getFormattedDate() const;
  String getDayOfWeek() const;
  String getTOD() const; // Time of Day (AM/PM)
  uint8_t getHour() const;

  bool is24HourFormat() const;

private:
  TimeManager() {} // Private constructor for singleton

  // Stores the last successful sync date in YYYYMMDD form. 0 = never synced.
  // Use an unsigned 32-bit integer to avoid sign issues and make the
  // range explicit (YYYYMMDD fits well within 32 bits).
  uint32_t lastSyncDate = 0;
  unsigned long lastUpdate = 0;
  static constexpr unsigned long UPDATE_INTERVAL = 1000; // 1 second

  void checkDailySync();
};