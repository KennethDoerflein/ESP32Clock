#pragma once
#include <Arduino.h>
#include <RTClib.h>

// Bitmask for days of the week
const uint8_t DAY_SUN = 1 << 0;
const uint8_t DAY_MON = 1 << 1;
const uint8_t DAY_TUE = 1 << 2;
const uint8_t DAY_WED = 1 << 3;
const uint8_t DAY_THU = 1 << 4;
const uint8_t DAY_FRI = 1 << 5;
const uint8_t DAY_SAT = 1 << 6;

// Define the snooze duration in milliseconds (e.g., 5 minutes)
#define SNOOZE_DURATION_MS (5 * 60 * 1000)

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
  unsigned long getSnoozeUntil() const { return _snoozeUntil; }

  // Public setters for properties
  void setId(uint8_t id) { _id = id; }
  void setEnabled(bool enabled) { _enabled = enabled; }
  void setHour(uint8_t hour) { _hour = hour; }
  void setMinute(uint8_t minute) { _minute = minute; }
  void setDays(uint8_t days) { _days = days; }
  void setSnoozeState(bool snoozed, unsigned long snoozeUntil)
  {
    _snoozed = snoozed;
    _snoozeUntil = snoozeUntil;
  }

  /**
   * @brief Snoozes the alarm for a predefined duration.
   */
  void snooze()
  {
    _snoozed = true;
    _snoozeUntil = millis() + SNOOZE_DURATION_MS;
  }

  /**
   * @brief Dismisses the alarm for the current day.
   * If it's a one-time alarm, it disables it permanently.
   */
  void dismiss()
  {
    _snoozed = false;
    _snoozeUntil = 0;
    if (_days == 0)
    {
      _enabled = false; // Disable one-time alarms
    }
  }

  /**
   * @brief Updates the snooze state of the alarm.
   * This should be called periodically.
   * @return true if the snooze period has just ended, false otherwise.
   */
  bool updateSnooze()
  {
    if (_snoozed && millis() > _snoozeUntil)
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
  unsigned long _snoozeUntil = 0;
};
