// AlarmManager.h

#pragma once

#include <Arduino.h>
#include "Alarm.h"
#include "TimeManager.h"

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
   * @brief Resumes an alarm that was ringing before a reboot.
   * @param alarmId The ID of the alarm to resume.
   * @param startTimestamp The Unix timestamp when the alarm originally started.
   */
  void resume(uint8_t alarmId, uint32_t startTimestamp);

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

  // --- Enums for the alarm state machine ---
  enum RampStage
  {
    STAGE_SLOW_BEEP,
    STAGE_FAST_BEEP,
    STAGE_CONTINUOUS
  };

  enum BuzzerState
  {
    BEEP_OFF,
    BEEP_ON
  };

  bool _isRinging;
  int _activeAlarmId;
  RampStage _rampStage;
  BuzzerState _buzzerState;
  uint32_t _alarmStartTimestamp; // Unix timestamp
  unsigned long _lastBeepTime;

  // --- Deferred Resume Logic ---
  bool _resumeAlarmOnBoot = false;
  uint8_t _pendingResumeAlarmId;
  uint32_t _pendingResumeTimestamp;
};
