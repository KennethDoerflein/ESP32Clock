// include/ButtonManager.h
#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

/**
 * @class ButtonManager
 * @brief Manages a physical button using interrupts to detect presses and their duration.
 *
 * This class encapsulates the logic for a hardware button, including debouncing
 * and interrupt handling. It is designed to be used with a button connected to
 * a GPIO pin, configured with an internal pull-up. The manager can be attached
 * or detached at runtime, allowing the button's function to be enabled or
 * disabled as needed.
 */
class ButtonManager
{
public:
  /**
   * @brief Constructs a new ButtonManager instance.
   * @param pin The GPIO pin to which the button is connected.
   */
  ButtonManager(int pin);

  /**
   * @brief Initializes the button manager and attaches the interrupt.
   */
  void begin();

  /**
   * @brief Attaches the interrupt handler to the button's pin.
   */
  void attach();

  /**
   * @brief Detaches the interrupt handler from the button's pin.
   */
  void detach();

  /**
   * @brief Gets the duration of the last button press.
   * @return The duration in milliseconds.
   */
  unsigned long getPressDuration();

  /**
   * @brief Checks if a new button press has been detected since the last check.
   * @return True if a new press is available, false otherwise.
   */
  bool newPressAvailable();

  /**
   * @brief Clears the flag indicating a new press is available.
   */
  void clearNewPress();

private:
  /**
   * @brief The Interrupt Service Routine (ISR) that handles button press events.
   * @param arg A pointer to the ButtonManager instance.
   */
  static void IRAM_ATTR handleInterrupt(void *arg);
  int _pin;
  volatile unsigned long _pressDuration;
  volatile bool _newPress;
  unsigned long _lastInterruptTime;
  unsigned long _buttonPressTime;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};

#endif // BUTTON_MANAGER_H
