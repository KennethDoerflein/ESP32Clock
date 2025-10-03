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

  bool is24HourFormat() const { return use24HourFormat; }
  void set24HourFormat(bool use24Hour) { use24HourFormat = use24Hour; }

private:
  TimeManager() {} // Private constructor for singleton

  bool use24HourFormat = false;
  bool hasSyncedToday = false;
  unsigned long lastUpdate = 0;
  static constexpr unsigned long UPDATE_INTERVAL = 1000; // 1 second

  void checkDailySync();
};