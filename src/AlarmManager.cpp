// AlarmManager.cpp

#include "AlarmManager.h"
#include "DisplayManager.h"

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
  // Simple beeping pattern: on for 200ms, off for 200ms.
  bool buzzerOn = (millis() % 400) < 200;
  digitalWrite(BUZZER_PIN, buzzerOn ? HIGH : LOW);
}

void AlarmManager::stop()
{
  if (!_isRinging)
    return;

  Serial.printf("Stopping alarm ID %d\n", _activeAlarmId);
  digitalWrite(BUZZER_PIN, LOW); // Turn off buzzer
  _isRinging = false;
  _activeAlarmId = -1;

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

  Serial.printf("Triggering alarm ID %d\n", alarmId);
  _isRinging = true;
  _activeAlarmId = alarmId;

  // Show the ringing screen
  DisplayManager::getInstance().showAlarmScreen();
}
