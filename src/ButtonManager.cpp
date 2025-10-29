// src/ButtonManager.cpp
#include "ButtonManager.h"
#include "constants.h"

ButtonManager::ButtonManager(int pin)
    : _pin(pin), _pressDuration(0), _newPress(false), _lastInterruptTime(0), _buttonPressTime(0)
{
}

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
  portENTER_CRITICAL_ISR(&instance->_mux);
  unsigned long interruptTime = millis();

  if (interruptTime - instance->_lastInterruptTime < DEBOUNCE_DELAY)
  {
    portEXIT_CRITICAL_ISR(&instance->_mux);
    return;
  }
  instance->_lastInterruptTime = interruptTime;

  if (digitalRead(instance->_pin) == LOW)
  {
    instance->_buttonPressTime = interruptTime;
  }
  else
  {
    if (instance->_buttonPressTime > 0)
    {
      instance->_pressDuration = interruptTime - instance->_buttonPressTime;
      instance->_newPress = true;
      instance->_buttonPressTime = 0;
    }
  }
  portEXIT_CRITICAL_ISR(&instance->_mux);
}

unsigned long ButtonManager::getPressDuration()
{
  portENTER_CRITICAL(&_mux);
  unsigned long duration = _pressDuration;
  portEXIT_CRITICAL(&_mux);
  return duration;
}

bool ButtonManager::newPressAvailable()
{
  portENTER_CRITICAL(&_mux);
  bool available = _newPress;
  portEXIT_CRITICAL(&_mux);
  return available;
}

void ButtonManager::clearNewPress()
{
  portENTER_CRITICAL(&_mux);
  _newPress = false;
  portEXIT_CRITICAL(&_mux);
}
