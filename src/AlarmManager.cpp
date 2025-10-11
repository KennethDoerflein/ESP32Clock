// AlarmManager.cpp

#include "AlarmManager.h"
#include "ConfigManager.h"
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

void AlarmManager::snooze()
{
  if (!_isRinging)
    return;

  Serial.printf("Snoozing alarm ID %d\n", _activeAlarmId);
  digitalWrite(BUZZER_PIN, LOW); // Turn off buzzer
  _isRinging = false;

  auto &config = ConfigManager::getInstance();
  Alarm snoozedAlarm = config.getAlarm(_activeAlarmId);
  snoozedAlarm.snoozed = true;
  snoozedAlarm.snoozeUntil = millis() + SNOOZE_DURATION_MS;
  config.setAlarm(_activeAlarmId, snoozedAlarm);

  _activeAlarmId = -1;

  // Force a redraw to clear the "RINGING!" message
  DisplayManager::getInstance().setPage(DisplayManager::getInstance().getCurrentPageIndex(), true);
}

void AlarmManager::dismiss()
{
  if (!_isRinging)
    return;

  Serial.printf("Dismissing alarm ID %d\n", _activeAlarmId);
  digitalWrite(BUZZER_PIN, LOW); // Turn off buzzer
  _isRinging = false;

  // For a repeating alarm, this just stops it for today.
  // For a one-time alarm, it's already been disabled by TimeManager.
  _activeAlarmId = -1;

  // Force a redraw to clear the "RINGING!" message
  DisplayManager::getInstance().setPage(DisplayManager::getInstance().getCurrentPageIndex(), true);
}

bool AlarmManager::isRinging() const
{
  return _isRinging;
}

void AlarmManager::trigger(const Alarm &alarm)
{
  if (_isRinging)
    return; // Don't trigger if another is already active

  Serial.printf("Triggering alarm ID %d\n", alarm.id);
  _isRinging = true;
  _activeAlarmId = alarm.id;

  // Show the ringing screen
  DisplayManager::getInstance().showAlarmScreen();
}
