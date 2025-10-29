#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "TimeManager.h"

// Bitmask for days of the week
const uint8_t DAY_SUN = 1 << 0;
const uint8_t DAY_MON = 1 << 1;
const uint8_t DAY_TUE = 1 << 2;
const uint8_t DAY_WED = 1 << 3;
const uint8_t DAY_THU = 1 << 4;
const uint8_t DAY_FRI = 1 << 5;
const uint8_t DAY_SAT = 1 << 6;

class Alarm
{
public:
  // Default constructor
  Alarm() = default;

  // Public getters for properties
  uint8_t getId() const { return _id; }
  bool isEnabled() const { return _enabled; }
  uint8_t getHour() const { return _hour; }
  uint8_t getMinute() const { return _minute; }
  uint8_t getDays() const { return _days; }
  bool isSnoozed() const { return _snoozed; }
  uint32_t getSnoozeUntil() const { return _snoozeUntil; }
  uint8_t getLastDismissedDay() const { return _lastDismissedDay; }

  // Public setters for properties
  void setId(uint8_t id) { _id = id; }
  void setEnabled(bool enabled)
  {
    _enabled = enabled;
    if (!_enabled)
    {
      _snoozed = false;
      _snoozeUntil = 0;
    }
  }
  void setHour(uint8_t hour) { _hour = hour; }
  void setMinute(uint8_t minute) { _minute = minute; }
  void setDays(uint8_t days) { _days = days; }
  void setSnoozeState(bool snoozed, uint32_t snoozeUntil)
  {
    _snoozed = snoozed;
    _snoozeUntil = snoozeUntil;
  }
  void setLastDismissedDay(uint8_t day) { _lastDismissedDay = day; }

  /**
   * @brief Snoozes the alarm for a user-defined duration.
   * @param snoozeMinutes The duration in minutes to snooze for.
   */
  void snooze(uint8_t snoozeMinutes)
  {
    _snoozed = true;
    // Use RTC unixtime for reboot resilience
    _snoozeUntil = TimeManager::getInstance().getRTCTime().unixtime() + (snoozeMinutes * 60);
  }

  /**
   * @brief Dismisses the alarm for the current day.
   * If it's a one-time alarm, it disables it permanently.
   * @param now The current time, used to record the dismiss day.
   */
  void dismiss(const DateTime &now)
  {
    _snoozed = false;
    _snoozeUntil = 0;
    if (_days == 0)
    {
      _enabled = false; // Disable one-time alarms
    }
    else
    {
      // For repeating alarms, record the day it was dismissed
      _lastDismissedDay = now.dayOfTheWeek();
    }
  }

  /**
   * @brief Updates the snooze state of the alarm.
   * This should be called periodically.
   * @return true if the snooze period has just ended, false otherwise.
   */
  bool updateSnooze()
  {
    if (_snoozed && TimeManager::getInstance().getRTCTime().unixtime() > _snoozeUntil)
    {
      _snoozed = false;
      _snoozeUntil = 0;
      return true; // Snooze is over
    }
    return false;
  }

  /**
   * @brief Checks if the alarm should be ringing at the given time.
   * @param now The current time.
   * @return true if the alarm should ring, false otherwise.
   */
  bool shouldRing(const DateTime &now) const
  {
    if (!_enabled || _snoozed)
    {
      return false;
    }

    // For repeating alarms, check if it has already been dismissed today.
    if (_days != 0 && _lastDismissedDay == now.dayOfTheWeek())
    {
      return false;
    }

    if (_hour == now.hour() && _minute == now.minute())
    {
      // Time matches. Check day of the week.
      uint8_t todayBitmask = 1 << now.dayOfTheWeek();

      // Ring if it's a one-time alarm or a repeating alarm for today.
      return (_days == 0 || (_days & todayBitmask) > 0);
    }
    return false;
  }

private:
  uint8_t _id = 0;
  bool _enabled = false;
  uint8_t _hour = 6; // Default to 6:00 AM
  uint8_t _minute = 0;
  uint8_t _days = 0; // Bitmask for days of the week
  bool _snoozed = false;
  uint32_t _snoozeUntil = 0;     // Unix timestamp for reboot resilience
  uint8_t _lastDismissedDay = 8; // 8 is an invalid day to ensure it can ring on first boot
};
