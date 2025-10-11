// AlarmManager.h

#pragma once

#include <Arduino.h>
#include "Alarm.h"

// Define the GPIO pin for the active buzzer
#define BUZZER_PIN 4

/**
 * @class AlarmManager
 * @brief Manages the physical ringing of an alarm (buzzer and display).
 *
 * This singleton class is a simple "ringer" service. It controls the buzzer,
 * coordinates with the DisplayManager to show the ringing screen, and keeps
 * track of the currently active alarm. The logic for snoozing and dismissing
 * is handled by the Alarm class itself.
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
   * @brief Updates the alarm manager's state (e.g., beeping). Should be in the main loop.
   */
  void update();

  /**
   * @brief Triggers the alarm to start ringing.
   * @param alarmId The ID of the alarm that is being triggered.
   */
  void trigger(uint8_t alarmId);

  /**
   * @brief Stops the alarm from ringing.
   */
  void stop();

  /**
   * @brief Checks if an alarm is currently ringing.
   * @return True if an alarm is active, false otherwise.
   */
  bool isRinging() const;

  /**
   * @brief Gets the ID of the currently ringing alarm.
   * @return The ID of the active alarm, or -1 if no alarm is ringing.
   */
  int getActiveAlarmId() const;

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  AlarmManager() : _isRinging(false), _activeAlarmId(-1) {}

  bool _isRinging;
  int _activeAlarmId;
};
