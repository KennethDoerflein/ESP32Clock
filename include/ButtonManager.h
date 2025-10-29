// include/ButtonManager.h
#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

class ButtonManager
{
public:
  ButtonManager(int pin);
  void begin();
  void attach();
  void detach();
  unsigned long getPressDuration();
  bool newPressAvailable();
  void clearNewPress();

private:
  static void IRAM_ATTR handleInterrupt(void *arg);
  int _pin;
  volatile unsigned long _pressDuration;
  volatile bool _newPress;
  unsigned long _lastInterruptTime;
  unsigned long _buttonPressTime;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};

#endif // BUTTON_MANAGER_H
