#pragma once

#include <Arduino.h>

class ConfigManager
{
public:
  static ConfigManager &getInstance()
  {
    static ConfigManager instance;
    return instance;
  }

  void begin();
  bool save();

  // Getters
  bool isAutoBrightness() const { return autoBrightness; }
  uint8_t getBrightness() const { return brightness; }
  bool is24HourFormat() const { return use24HourFormat; }
  bool isCelsius() const { return useCelsius; }

  // Setters
  void setAutoBrightness(bool enabled) { autoBrightness = enabled; }
  void setBrightness(uint8_t value) { brightness = value; }
  void set24HourFormat(bool enabled) { use24HourFormat = enabled; }
  void setCelsius(bool enabled) { useCelsius = enabled; }

private:
  ConfigManager() {} // Private constructor for singleton

  // Default settings
  bool autoBrightness = true;
  uint8_t brightness = 128;
  bool use24HourFormat = false;
  bool useCelsius = false;

  void load();
  void setDefaults();
};