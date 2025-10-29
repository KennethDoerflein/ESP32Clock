// src/ButtonManager.cpp
#include "ButtonManager.h"
#include "constants.h"

unsigned long ButtonManager::s_lastInterruptTime = 0;
unsigned long ButtonManager::s_buttonPressTime = 0;

ButtonManager::ButtonManager(int pin) : _pin(pin), _pressDuration(0), _newPress(false) {}

void ButtonManager::begin()
{
  pinMode(_pin, INPUT_PULLUP);
  attach();
}

void ButtonManager::attach()
{
  attachInterruptArg(digitalPinToInterrupt(_pin), handleInterrupt, this, CHANGE);
}

void ButtonManager::detach()
{
  detachInterrupt(digitalPinToInterrupt(_pin));
}

void IRAM_ATTR ButtonManager::handleInterrupt(void *arg)
{
  ButtonManager *instance = static_cast<ButtonManager *>(arg);
  unsigned long interruptTime = millis();

  if (interruptTime - s_lastInterruptTime < DEBOUNCE_DELAY)
  {
    return;
  }
  s_lastInterruptTime = interruptTime;

  if (digitalRead(instance->_pin) == LOW)
  {
    s_buttonPressTime = interruptTime;
  }
  else
  {
    if (s_buttonPressTime > 0)
    {
      instance->_pressDuration = interruptTime - s_buttonPressTime;
      instance->_newPress = true;
      s_buttonPressTime = 0;
    }
  }
}

unsigned long ButtonManager::getPressDuration()
{
  return _pressDuration;
}

bool ButtonManager::newPressAvailable()
{
  return _newPress;
}

void ButtonManager::clearNewPress()
{
  _newPress = false;
}
