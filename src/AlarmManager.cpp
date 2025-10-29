// AlarmManager.cpp

#include "AlarmManager.h"
#include "display.h"
#include "DisplayManager.h"
#include "SerialLog.h"
#include "ConfigManager.h"
#include "constants.h"

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
    digitalWrite(BUZZER_PIN, HIGH); // Turn buzzer on permanently for this stage
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
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
  else // BEEP_OFF
  {
    if (beepElapsedTime >= beepOffDuration)
    {
      _buzzerState = BEEP_ON;
      _lastBeepTime = currentTime;
      digitalWrite(BUZZER_PIN, HIGH);
    }
  }
}

void AlarmManager::stop()
{
  if (!_isRinging)
    return;

  SerialLog::getInstance().printf("Stopping alarm ID %d\n", _activeAlarmId);
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
  _isRinging = false;
  _activeAlarmId = -1;

  // --- Clear the persisted ringing state ---
  auto &config = ConfigManager::getInstance();
  config.setRingingAlarmId(-1);
  config.setRingingAlarmStartTimestamp(0);
  config.save(); // Persist the change immediately

  // --- Reset state machine ---
  _rampStage = STAGE_SLOW_BEEP;
  _buzzerState = BEEP_OFF;

  Display::getInstance().setBacklightFlashing(false);
}

bool AlarmManager::isRinging() const
{
  return _isRinging;
}

int AlarmManager::getActiveAlarmId() const
{
  return _activeAlarmId;
}

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
  digitalWrite(BUZZER_PIN, HIGH); // Start with the buzzer on
  _isRinging = true;
  _activeAlarmId = alarmId;

  // --- Persist the ringing state ---
  auto &config = ConfigManager::getInstance();
  config.setRingingAlarmId(alarmId);
  config.setRingingAlarmStartTimestamp(_alarmStartTimestamp);
  config.save(); // Persist the change immediately

  // Switch to the clock page before showing the alarm overlay
  DisplayManager::getInstance().setPage(0);
  Display::getInstance().setBacklightFlashing(true);
}

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
  digitalWrite(BUZZER_PIN, HIGH);

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

  // Switch to the clock page before showing the alarm overlay
  DisplayManager::getInstance().setPage(0);
  Display::getInstance().setBacklightFlashing(true);
}
