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

/**
 * @class Alarm
 * @brief Represents a single alarm, handling its state, schedule, and logic.
 *
 * This class encapsulates all properties of an alarm, including its time,
 * repeat days, and whether it's enabled, snoozed, or dismissed. It provides
 * methods to manage the alarm's lifecycle, from snoozing and dismissing to
 * checking if it should be ringing.
 */
class Alarm
{
public:
  /**
   * @brief Default constructor for creating an Alarm instance.
   */
  Alarm() = default;

  /**
   * @brief Computes whether the given DateTime falls in an odd ISO week.
   * @param dt The DateTime to evaluate.
   * @return True if the ISO week number is odd, false if even.
   */
  static bool isOddWeek(const DateTime &dt)
  {
    // A robust, continuous week parity calculation that never flips mid-week
    // or repeats at year boundaries (unlike ISO week numbers).
    // Epoch (1970-01-01) was a Thursday.
    // Monday, Jan 5, 1970 is 4 days after epoch.
    // By subtracting 4 from days since epoch, we align week 0 to start on a Monday.
    uint32_t daysSinceEpoch = dt.unixtime() / 86400;
    int32_t absoluteWeek = ((int32_t)daysSinceEpoch - 4) / 7;
    return (absoluteWeek % 2) == 1;
  }

  /**
   * @brief Gets the unique identifier of the alarm.
   * @return The alarm's ID.
   */
  uint8_t getId() const { return _id; }

  /**
   * @brief Checks if the alarm is enabled.
   * @return True if the alarm is enabled, false otherwise.
   */
  bool isEnabled() const { return _enabled; }

  /**
   * @brief Gets the hour at which the alarm is set to ring.
   * @return The alarm hour (0-23).
   */
  uint8_t getHour() const { return _hour; }

  /**
   * @brief Gets the minute at which the alarm is set to ring.
   * @return The alarm minute (0-59).
   */
  uint8_t getMinute() const { return _minute; }

  /**
   * @brief Gets the bitmask representing the days of the week the alarm repeats.
   * @return A bitmask where each bit corresponds to a day (e.g., DAY_MON).
   */
  uint8_t getDays() const { return _days; }

  /**
   * @brief Checks if the alarm is set to biweekly (every other week).
   * @return True if biweekly mode is enabled, false otherwise.
   */
  bool isBiweekly() const { return _biweekly; }

  /**
   * @brief Gets the week parity the biweekly alarm should fire on.
   * @return True if the alarm fires on odd ISO weeks, false for even.
   */
  bool isBiweeklyOddWeek() const { return _biweeklyOddWeek; }

  /**
   * @brief Checks if the alarm is currently snoozed.
   * @return True if the alarm is snoozed, false otherwise.
   */
  bool isSnoozed() const { return _snoozed; }

  /**
   * @brief Gets the Unix timestamp until which the alarm is snoozed.
   * @return The snooze end timestamp, or 0 if not snoozed.
   */
  uint32_t getSnoozeUntil() const { return _snoozeUntil; }

  /**
   * @brief Gets the epoch day the alarm was last dismissed.
   * @return The days since Unix epoch, or 0 if not dismissed.
   */
  uint32_t getLastDismissedDayEpoch() const { return _lastDismissedDayEpoch; }

  /**
   * @brief Sets the unique identifier of the alarm.
   * @param id The new ID for the alarm.
   */
  void setId(uint8_t id) { _id = id; }

  /**
   * @brief Enables or disables the alarm. Disabling an alarm also clears its snooze state.
   * @param enabled True to enable the alarm, false to disable it.
   */
  void setEnabled(bool enabled)
  {
    _enabled = enabled;
    if (!_enabled)
    {
      _snoozed = false;
      _snoozeUntil = 0;
    }
  }

  /**
   * @brief Sets the hour at which the alarm should ring.
   * @param hour The new hour for the alarm (0-23).
   */
  void setHour(uint8_t hour) { _hour = (hour > 23) ? 23 : hour; }

  /**
   * @brief Sets the minute at which the alarm should ring.
   * @param minute The new minute for the alarm (0-59).
   */
  void setMinute(uint8_t minute) { _minute = (minute > 59) ? 59 : minute; }

  /**
   * @brief Sets the days of the week for the alarm to repeat.
   * @param days A bitmask representing the days (e.g., DAY_MON | DAY_WED).
   */
  void setDays(uint8_t days) { _days = days; }

  /**
   * @brief Enables or disables biweekly (every other week) mode.
   * @param biweekly True to enable biweekly mode, false for every week.
   */
  void setBiweekly(bool biweekly) { _biweekly = biweekly; }

  /**
   * @brief Sets the week parity the biweekly alarm should fire on.
   * @param oddWeek True for odd ISO weeks, false for even.
   */
  void setBiweeklyOddWeek(bool oddWeek) { _biweeklyOddWeek = oddWeek; }

  /**
   * @brief Manually sets the snooze state of the alarm.
   * @param snoozed True to set the alarm as snoozed, false otherwise.
   * @param snoozeUntil The Unix timestamp when the snooze period ends.
   */
  void setSnoozeState(bool snoozed, uint32_t snoozeUntil)
  {
    _snoozed = snoozed;
    _snoozeUntil = snoozeUntil;
  }

  /**
   * @brief Sets the last day the alarm was dismissed as an epoch day.
   * @param dayEpoch The days since Unix epoch.
   */
  void setLastDismissedDayEpoch(uint32_t dayEpoch) { _lastDismissedDayEpoch = dayEpoch; }

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
      // For repeating alarms, record the absolute day it was dismissed
      _lastDismissedDayEpoch = now.unixtime() / 86400;
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
    if (_days != 0 && _lastDismissedDayEpoch == now.unixtime() / 86400)
    {
      return false;
    }

    if (_hour == now.hour() && _minute == now.minute())
    {
      // Time matches. Check day of the week.
      uint8_t todayBitmask = 1 << now.dayOfTheWeek();

      if (_days == 0)
      {
        return true; // One-time alarm
      }

      if ((_days & todayBitmask) == 0)
      {
        return false; // Not scheduled for today's day of week
      }

      // For biweekly alarms, check if this is the correct week.
      if (_biweekly && isOddWeek(now) != _biweeklyOddWeek)
      {
        return false; // Wrong week parity
      }

      return true;
    }
    return false;
  }

private:
  uint8_t _id = 0;
  bool _enabled = false;
  uint8_t _hour = 6; // Default to 6:00 AM
  uint8_t _minute = 0;
  uint8_t _days = 0;          // Bitmask for days of the week
  bool _biweekly = false;     // Fire every other week instead of every week
  bool _biweeklyOddWeek = false; // Which week parity to fire on (true=odd, false=even)
  bool _snoozed = false;
  uint32_t _snoozeUntil = 0;     // Unix timestamp for reboot resilience
  uint32_t _lastDismissedDayEpoch = 0; // Days since epoch it was last dismissed
};
