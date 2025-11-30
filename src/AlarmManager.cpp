/**
 * @file AlarmManager.cpp
 * @brief Implements the AlarmManager class for handling alarm ringing logic.
 *
 * This file contains the implementation of the AlarmManager, which controls
 * the physical buzzer, manages the alarm sound progression (ramping), and
 * handles auto-shutoff and resuming alarms after a reboot.
 */

#include "AlarmManager.h"
#include "Display.h"
#include "DisplayManager.h"
#include "SerialLog.h"
#include "ConfigManager.h"
#include "Constants.h"

/**
 * @brief Initializes the AlarmManager.
 *
 * Sets up the buzzer pin and checks if an alarm was ringing when the device
 * was last powered off. If so, it schedules a deferred resume of the alarm.
 */
void AlarmManager::begin()
{
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // --- Check for a ringing alarm at startup ---
  int8_t ringingAlarmId = ConfigManager::getInstance().getRingingAlarmId();
  if (ringingAlarmId != -1)
  {
    _resumeAlarmOnBoot = true;
    _pendingResumeAlarmId = ringingAlarmId;
    _pendingResumeTimestamp = ConfigManager::getInstance().getRingingAlarmStartTimestamp();
    SerialLog::getInstance().print("AlarmManager: Pending resume for alarm.\n");
  }
}

/**
 * @brief Updates the state of the ringing alarm.
 *
 * This method should be called in the main loop. It handles the progressive
 * ramping of the alarm sound, from slow beeps to a continuous tone. It also
 * contains the logic for automatically shutting off the alarm after a prolonged
 * period.
 */
void AlarmManager::update()
{
  // --- Handle deferred resume first ---
  // To ensure the system is fully stable, especially the display driver,
  // the deferred resume operation is delayed for a few seconds after boot.
  if (_resumeAlarmOnBoot && millis() > ALARM_RESUME_DELAY_MS)
  {
    resume(_pendingResumeAlarmId, _pendingResumeTimestamp);
    _resumeAlarmOnBoot = false; // Clear the flag
  }

  if (!_isRinging)
  {
    return;
  }

  // --- Stage Progression Logic ---
  uint32_t now = TimeManager::getInstance().getRTCTime().unixtime();
  uint32_t alarmElapsedSeconds = now - _alarmStartTimestamp;

  // --- Auto-off Logic ---
  if (alarmElapsedSeconds >= ALARM_AUTO_OFF_SECONDS)
  {
    SerialLog::getInstance().print("AlarmManager: Auto-stopping alarm after 30 minutes.\n");
    stop();
    return;
  }

  if (_rampStage == STAGE_SLOW_BEEP && alarmElapsedSeconds >= (STAGE1_DURATION_MS / 1000))
  {
    SerialLog::getInstance().print("AlarmManager: Ramping to STAGE_FAST_BEEP\n");
    _rampStage = STAGE_FAST_BEEP;
  }
  else if (_rampStage == STAGE_FAST_BEEP && alarmElapsedSeconds >= ((STAGE1_DURATION_MS + STAGE2_DURATION_MS) / 1000))
  {
    SerialLog::getInstance().print("AlarmManager: Ramping to STAGE_CONTINUOUS\n");
    _rampStage = STAGE_CONTINUOUS;
    //digitalWrite(BUZZER_PIN, HIGH); // Turn buzzer on permanently for this stage
    return;                         // Skip beeping logic
  }

  if (_rampStage == STAGE_CONTINUOUS)
  {
    return; // Buzzer is already on
  }

  // --- Beeping Logic ---
  unsigned long currentTime = millis();
  unsigned long beepOnDuration = (_rampStage == STAGE_SLOW_BEEP) ? SLOW_BEEP_ON_MS : FAST_BEEP_ON_MS;
  unsigned long beepOffDuration = (_rampStage == STAGE_SLOW_BEEP) ? SLOW_BEEP_OFF_MS : FAST_BEEP_OFF_MS;
  unsigned long beepElapsedTime = currentTime - _lastBeepTime;

  if (_buzzerState == BEEP_ON)
  {
    if (beepElapsedTime >= beepOnDuration)
    {
      _buzzerState = BEEP_OFF;
      _lastBeepTime = currentTime;
      //digitalWrite(BUZZER_PIN, LOW);
    }
  }
  else // BEEP_OFF
  {
    if (beepElapsedTime >= beepOffDuration)
    {
      _buzzerState = BEEP_ON;
      _lastBeepTime = currentTime;
      //digitalWrite(BUZZER_PIN, HIGH);
    }
  }
}

/**
 * @brief Stops the currently ringing alarm.
 *
 * This function deactivates the buzzer, resets the alarm state machine,
 * and clears the persisted ringing state from the configuration. It also
 * stops the backlight from flashing.
 */
void AlarmManager::stop()
{
  if (!_isRinging)
    return;

  SerialLog::getInstance().printf("Stopping alarm ID %d\n", _activeAlarmId);
  //digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
  _isRinging = false;
  _activeAlarmId = -1;

  // --- Clear the persisted ringing state ---
  auto &config = ConfigManager::getInstance();
  config.setRingingAlarmId(-1);
  config.setRingingAlarmStartTimestamp(0);
  config.saveRingingAlarmState(); // Persist the change immediately

  // --- Reset state machine ---
  _rampStage = STAGE_SLOW_BEEP;
  _buzzerState = BEEP_OFF;

  Display::getInstance().setBacklightFlashing(false);

  // Refresh display to clear alarm overlay
  DisplayManager::getInstance().requestPartialRefresh();
}

/**
 * @brief Checks if an alarm is currently ringing.
 * @return True if an alarm is active, false otherwise.
 */
bool AlarmManager::isRinging() const
{
  return _isRinging;
}

/**
 * @brief Gets the ID of the currently active alarm.
 * @return The ID of the alarm, or -1 if no alarm is ringing.
 */
int AlarmManager::getActiveAlarmId() const
{
  return _activeAlarmId;
}

/**
 * @brief Triggers a new alarm to start ringing.
 *
 * This function initiates the alarm sequence. It sets up the state machine
 * for the ramping sound, persists the ringing state to handle reboots, and
 * activates the visual feedback on the display.
 *
 * @param alarmId The ID of the alarm to trigger.
 */
void AlarmManager::trigger(uint8_t alarmId)
{
  if (_isRinging)
    return; // Don't trigger if another is already active

  SerialLog::getInstance().printf("Triggering alarm ID %d\n", alarmId);

  // --- Initialize the ramping alarm state ---
  _alarmStartTimestamp = TimeManager::getInstance().getRTCTime().unixtime();
  _lastBeepTime = millis();
  _rampStage = STAGE_SLOW_BEEP;
  _buzzerState = BEEP_ON;
  //digitalWrite(BUZZER_PIN, HIGH); // Start with the buzzer on
  _isRinging = true;
  _activeAlarmId = alarmId;

  // --- Persist the ringing state ---
  auto &config = ConfigManager::getInstance();
  config.setRingingAlarmId(alarmId);
  config.setRingingAlarmStartTimestamp(_alarmStartTimestamp);
  config.saveRingingAlarmState(); // Persist the change immediately

  // Flash backlight
  Display::getInstance().setBacklightFlashing(true);

  // Update display immediately to show alarm overlay
  DisplayManager::getInstance().requestPartialRefresh();
}

/**
 * @brief Resumes an alarm that was ringing before a reboot.
 *
 * This function is called at startup if a ringing alarm was detected in
 * the saved configuration. It recalculates the current ramp stage based on
 * the original start time and continues the alarm sequence.
 *
 * @param alarmId The ID of the alarm to resume.
 * @param startTimestamp The original Unix timestamp when the alarm started.
 */
void AlarmManager::resume(uint8_t alarmId, uint32_t startTimestamp)
{
  if (_isRinging)
    return;

  SerialLog::getInstance().printf("Resuming ringing alarm ID %d\n", alarmId);
  _isRinging = true;
  _activeAlarmId = alarmId;
  _alarmStartTimestamp = startTimestamp;

  // We don't know the buzzer state from before the reboot, so start it
  _lastBeepTime = millis();
  _buzzerState = BEEP_ON;
  //digitalWrite(BUZZER_PIN, HIGH);

  // Re-evaluate the ramp stage based on how long it's been ringing.
  uint32_t now = TimeManager::getInstance().getRTCTime().unixtime();
  uint32_t alarmElapsedSeconds = now - _alarmStartTimestamp;

  if (alarmElapsedSeconds >= ((STAGE1_DURATION_MS + STAGE2_DURATION_MS) / 1000))
  {
    _rampStage = STAGE_CONTINUOUS;
  }
  else if (alarmElapsedSeconds >= (STAGE1_DURATION_MS / 1000))
  {
    _rampStage = STAGE_FAST_BEEP;
  }
  else
  {
    _rampStage = STAGE_SLOW_BEEP;
  }
  SerialLog::getInstance().printf("Resumed at ramp stage %d\n", _rampStage);

  // Flash backlight
  Display::getInstance().setBacklightFlashing(true);

  // Update display immediately to show alarm overlay
  DisplayManager::getInstance().requestPartialRefresh();
}
