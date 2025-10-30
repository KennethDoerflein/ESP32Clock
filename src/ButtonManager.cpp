/**
 * @file ButtonManager.cpp
 * @brief Implements the ButtonManager class for handling hardware button interrupts.
 *
 * This file contains the implementation for the ButtonManager, which uses
 * hardware interrupts to detect button presses and releases, calculates the
 * press duration, and provides a debounced interface for the main application.
 */
#include "ButtonManager.h"
#include "Constants.h"

/**
 * @brief Constructs a new ButtonManager.
 * @param pin The GPIO pin the button is connected to.
 */
ButtonManager::ButtonManager(int pin)
    : _pin(pin), _pressDuration(0), _newPress(false), _lastInterruptTime(0), _buttonPressTime(0)
{
}

/**
 * @brief Initializes the ButtonManager.
 *
 * Sets the button's GPIO pin to input with a pull-up resistor and attaches
 * the interrupt handler.
 */
void ButtonManager::begin()
{
  pinMode(_pin, INPUT_PULLUP);
  attach();
}

/**
 * @brief Attaches the interrupt handler to the button's pin.
 *
 * This allows the button to trigger events. It is called automatically by `begin()`.
 */
void ButtonManager::attach()
{
  attachInterruptArg(digitalPinToInterrupt(_pin), handleInterrupt, this, CHANGE);
}

/**
 * @brief Detaches the interrupt handler from the button's pin.
 *
 * This disables the button's ability to trigger events, which is useful when
 * polling is required (e.g., when an alarm is ringing).
 */
void ButtonManager::detach()
{
  detachInterrupt(digitalPinToInterrupt(_pin));
}

/**
 * @brief The Interrupt Service Routine (ISR) for the button.
 *
 * This static method is called by the ESP32's interrupt system. It handles
 * debouncing and records the press and release times to calculate the duration.
 * This method should not be called directly.
 *
 * @param arg A void pointer to the ButtonManager instance.
 */
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

/**
 * @brief Gets the duration of the last recorded button press.
 *
 * This function is thread-safe and can be called from the main loop.
 *
 * @return The press duration in milliseconds.
 */
unsigned long ButtonManager::getPressDuration()
{
  portENTER_CRITICAL(&_mux);
  unsigned long duration = _pressDuration;
  portEXIT_CRITICAL(&_mux);
  return duration;
}

/**
 * @brief Checks if a new button press has been registered.
 *
 * This function is thread-safe. It returns true if a new press has occurred
 * since the last time `clearNewPress()` was called.
 *
 * @return True if a new press is available, false otherwise.
 */
bool ButtonManager::newPressAvailable()
{
  portENTER_CRITICAL(&_mux);
  bool available = _newPress;
  portEXIT_CRITICAL(&_mux);
  return available;
}

/**
 * @brief Clears the new press flag.
 *
 * This should be called after processing a new button press to reset the state.
 * This function is thread-safe.
 */
void ButtonManager::clearNewPress()
{
  portENTER_CRITICAL(&_mux);
  _newPress = false;
  portEXIT_CRITICAL(&_mux);
}
