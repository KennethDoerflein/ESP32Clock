#pragma once
#include <Arduino.h>

// Bitmask for days of the week
const uint8_t DAY_SUN = 1 << 0;
const uint8_t DAY_MON = 1 << 1;
const uint8_t DAY_TUE = 1 << 2;
const uint8_t DAY_WED = 1 << 3;
const uint8_t DAY_THU = 1 << 4;
const uint8_t DAY_FRI = 1 << 5;
const uint8_t DAY_SAT = 1 << 6;

struct Alarm
{
  uint8_t id = 0;
  bool enabled = false;
  uint8_t hour = 8;
  uint8_t minute = 0;
  uint8_t days = 0; // Bitmask for days of the week
  bool snoozed = false;
  unsigned long snoozeUntil = 0;
};
