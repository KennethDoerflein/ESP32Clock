// AlarmManager.cpp

#include "AlarmManager.h"
#include "DisplayManager.h"
#include "SerialLog.h"

// --- Constants for the ramping alarm state machine ---
const unsigned long STAGE1_DURATION_MS = 10000; // 10 seconds of slow beeping
const unsigned long STAGE2_DURATION_MS = 20000; // 20 seconds of fast beeping
// After STAGE2, the alarm will be continuous.

// Beep timings for each stage
const unsigned long SLOW_BEEP_ON_MS = 200;
const unsigned long SLOW_BEEP_OFF_MS = 800;
const unsigned long FAST_BEEP_ON_MS = 150;
const unsigned long FAST_BEEP_OFF_MS = 150;

void AlarmManager::begin()
{
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void AlarmManager::update()
{
  if (!_isRinging)
  {
    return;
  }

  unsigned long currentTime = millis();
  unsigned long alarmElapsedTime = currentTime - _alarmStartTime;

  // --- Stage Progression Logic ---
  if (_rampStage == STAGE_SLOW_BEEP && alarmElapsedTime >= STAGE1_DURATION_MS)
  {
    SerialLog::getInstance().print("AlarmManager: Ramping to STAGE_FAST_BEEP\n");
    _rampStage = STAGE_FAST_BEEP;
  }
  else if (_rampStage == STAGE_FAST_BEEP && alarmElapsedTime >= (STAGE1_DURATION_MS + STAGE2_DURATION_MS))
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

  // --- Reset state machine ---
  _rampStage = STAGE_SLOW_BEEP;
  _buzzerState = BEEP_OFF;

  // Force a redraw to clear the "RINGING!" message
  DisplayManager::getInstance().setPage(DisplayManager::getInstance().getCurrentPageIndex(), true);
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
  _isRinging = true;
  _activeAlarmId = alarmId;

  // --- Initialize the ramping alarm state ---
  _alarmStartTime = millis();
  _lastBeepTime = _alarmStartTime;
  _rampStage = STAGE_SLOW_BEEP;
  _buzzerState = BEEP_ON;
  digitalWrite(BUZZER_PIN, HIGH); // Start with the buzzer on

  // Show the ringing screen
  DisplayManager::getInstance().showAlarmScreen();
}
