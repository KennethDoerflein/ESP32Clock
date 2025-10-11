// AlarmManager.h

#pragma once

#include <Arduino.h>
#include "Alarm.h"

// Define the GPIO pin for the active buzzer
#define BUZZER_PIN 4
// Define the snooze duration in milliseconds (e.g., 5 minutes)
#define SNOOZE_DURATION_MS (5 * 60 * 1000)

/**
 * @class AlarmManager
 * @brief Manages the state of a ringing alarm, including audio and display feedback.
 *
 * This singleton class is responsible for what happens when an alarm is triggered.
 * It controls the buzzer, manages the snooze and dismiss logic, and coordinates
 * with the DisplayManager to show the appropriate screen.
 */
class AlarmManager
{
public:
  /**
   * @brief Gets the singleton instance of the AlarmManager.
   * @return A reference to the singleton AlarmManager instance.
   */
  static AlarmManager &getInstance()
  {
    static AlarmManager instance;
    return instance;
  }

  /**
   * @brief Initializes the AlarmManager.
   */
  void begin();

  /**
   * @brief Updates the alarm manager's state. Should be called in the main loop.
   */
  void update();

  /**
   * @brief Triggers the alarm to start ringing.
   * @param alarm The alarm that is being triggered.
   */
  void trigger(const Alarm &alarm);

  /**
   * @brief Snoozes the currently ringing alarm.
   */
  void snooze();

  /**
   * @brief Dismisses the currently ringing alarm for the day.
   */
  void dismiss();

  /**
   * @brief Checks if an alarm is currently ringing.
   * @return True if an alarm is active, false otherwise.
   */
  bool isRinging() const;

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  AlarmManager() : _isRinging(false), _activeAlarmId(-1) {}

  bool _isRinging;
  int _activeAlarmId;
};
