// TimeManager.h

#pragma once

#include <Arduino.h>
#include <RTClib.h>

/**
 * @class TimeManager
 * @brief Manages timekeeping, NTP synchronization, and time formatting.
 *
 * This singleton class handles all time-related functions for the clock,
 * including initializing the Real-Time Clock (RTC), syncing with an NTP server,
 * and providing formatted time and date strings for display.
 */
class TimeManager
{
public:
  /**
   * @brief Gets the singleton instance of the TimeManager.
   * @return A reference to the singleton TimeManager instance.
   */
  static TimeManager &getInstance()
  {
    static TimeManager instance;
    return instance;
  }

  /**
   * @brief Initializes the TimeManager.
   *
   * This should be called once during setup. It triggers the initial
   * NTP time synchronization.
   */
  void begin();

  /**
   * @brief Updates the time manager's state.
   *
   * This method should be called repeatedly in the main loop. It handles
   * periodic tasks, such as the daily NTP sync check.
   * @return true if a time update occurred (typically once per second).
   */
  bool update();

  /**
   * @brief Checks all alarms to see if any snoozed alarms should be re-triggered.
   */
  void updateSnoozeStates();

  /**
   * @brief Forces a time synchronization with the NTP server.
   *
   * This function attempts to connect to an NTP server to update the RTC.
   * On success, it records the date of the sync.
   */
  void syncWithNTP();

  /**
   * @brief Drives the non-blocking NTP state machine.
   * @details This should be called in the main application loop to handle
   * ongoing asynchronous NTP synchronization attempts.
   * @return true if the time was updated during this call.
   */
  bool updateNtp();

  /**
   * @brief Gets the current time, formatted for display.
   * @return A String containing the formatted time (e.g., "10:30" or "22:30").
   */
  String getFormattedTime() const;

  /**
   * @brief Gets the current date, formatted for display.
   * @return A String containing the formatted date (e.g., "JAN 1").
   */
  String getFormattedDate() const;

  /**
   * @brief Gets the current day of the week.
   * @return A three-letter String for the day of the week (e.g., "MON").
   */
  String getDayOfWeek() const;

  /**
   * @brief Gets the time of day period (AM/PM).
   * @return "AM", "PM", or an empty string if in 24-hour format.
   */
  String getTOD() const;

  /**
   * @brief Gets the seconds of the current time, formatted for display.
   * @return A String containing the formatted seconds (e.g., "59").
   */
  String getFormattedSeconds() const;

  /**
   * @brief Gets the current hour in 24-hour format.
   * @return The current hour (0-23).
   */
  uint8_t getHour() const;

  /**
   * @brief Checks if the time should be displayed in 24-hour format.
   * @return True if 24-hour format is active, false otherwise.
   */
  bool is24HourFormat() const;

  /**
   * @brief Gets the current time from the RTC.
   * @return A DateTime object representing the current time.
   */
  DateTime getRTCTime() const;

  /**
   * @brief Checks if the RTC has been set to a valid time.
   * @return True if the time is valid (year > 2000), false otherwise.
   */
  bool isTimeSet() const;

  /**
   * @brief Checks if a daily NTP sync is needed and performs it.
   *
   * This is typically run at a specific time of day (e.g., 3 AM) to ensure
   * the clock remains accurate.
   */
  void checkDailySync();

  /**
   * @brief Periodically checks RTC drift against NTP and resyncs if needed.
   *
   * This function is designed to be called in the main loop. It checks the
   * time drift at a defined interval (e.g., every 4 hours) and triggers a
   * non-blocking NTP sync if the drift exceeds a specified threshold.
   */
  void checkDriftAndResync();

  /**
   * @brief Checks for DST transitions and updates the RTC if needed.
   */
  void checkDST();

  /**
   * @brief Processes the alarm event outside of the ISR.
   */
  void handleAlarm();

  /**
   * @brief Sets the RTC's hardware alarms for the next two upcoming alarms.
   */
  void setNextAlarms();

  /**
   * @brief Checks for any alarms that were missed while the device was off.
   * @note This is intended to be called once at boot.
   */
  void checkMissedAlarms();

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  TimeManager() {}

  /// @brief Stores the date of the last successful NTP sync in YYYYMMDD format. 0 = never synced.
  uint32_t lastSyncDate = 0;

  /// @brief Timestamp of the last update call to manage update intervals.
  unsigned long lastUpdate = 0;

  /// @brief The interval at which the `update` method runs its checks, in milliseconds.
  static constexpr unsigned long UPDATE_INTERVAL = 1000; // 1 second

  /// @brief Timestamp of the last drift check.
  unsigned long lastDriftCheck = 0;

  bool _rtc_alarms_initialized = false;
  int8_t _rtcAlarm1Id = -1; ///< The ID of the alarm associated with RTC alarm 1.
  int8_t _rtcAlarm2Id = -1; ///< The ID of the alarm associated with RTC alarm 2.

  /**
   * @brief Clears both hardware alarms on the RTC.
   */
  void clearRtcAlarms();
};
