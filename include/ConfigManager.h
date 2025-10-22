// ConfigManager.h

#pragma once

#include <Arduino.h>
#include "Alarm.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

const int MAX_ALARMS = 5; // Maximum number of alarms that can be set

/**
 * @class ConfigManager
 * @brief Manages the application's configuration settings using a singleton pattern.
 *
 * This class is responsible for loading, saving, and providing access to
 * configuration settings such as WiFi credentials, display preferences, and
 * other system options. Settings are stored in a JSON file on the LittleFS filesystem.
 */
class ConfigManager
{
public:
  // Default Colors
  static constexpr const char *DEFAULT_BACKGROUND_COLOR = "#000000";
  static constexpr const char *DEFAULT_TIME_COLOR = "#079ae4";
  static constexpr const char *DEFAULT_TOD_COLOR = "#FFFFFF";
  static constexpr const char *DEFAULT_SECONDS_COLOR = "#02e3ab";
  static constexpr const char *DEFAULT_DAY_OF_WEEK_COLOR = "#FFFFFF";
  static constexpr const char *DEFAULT_DATE_COLOR = "#FFFFFF";
  static constexpr const char *DEFAULT_TEMP_COLOR = "#02e3ab";
  static constexpr const char *DEFAULT_HUMIDITY_COLOR = "#079ae4";
  static constexpr const char *DEFAULT_ALARM_ICON_COLOR = "#FFFF00";
  static constexpr const char *DEFAULT_SNOOZE_ICON_COLOR = "#0000FF";
  static constexpr const char *DEFAULT_ALARM_TEXT_COLOR = "#FF0000";
  static constexpr const char *DEFAULT_ERROR_TEXT_COLOR = "#FF0000";

  // Default System Settings
  static constexpr const char *DEFAULT_WIFI_SSID = "";
  static constexpr const char *DEFAULT_WIFI_PASSWORD = "";
  static constexpr const char *DEFAULT_HOSTNAME = "";
  static constexpr bool DEFAULT_WIFI_CREDS_VALID = false;
  static constexpr bool DEFAULT_AUTO_BRIGHTNESS = true;
  static constexpr uint8_t DEFAULT_BRIGHTNESS = 128;
  static constexpr uint8_t DEFAULT_AUTO_BRIGHTNESS_START_HOUR = 7;
  static constexpr uint8_t DEFAULT_AUTO_BRIGHTNESS_END_HOUR = 21;
  static constexpr uint8_t DEFAULT_DAY_BRIGHTNESS = 255;
  static constexpr uint8_t DEFAULT_NIGHT_BRIGHTNESS = 10;
  static constexpr bool DEFAULT_USE_24_HOUR_FORMAT = false;
  static constexpr bool DEFAULT_USE_CELSIUS = false;
  static constexpr bool DEFAULT_SCREEN_FLIPPED = false;

  /**
   * @brief Gets the singleton instance of the ConfigManager.
   * @return A reference to the singleton ConfigManager instance.
   */
  static ConfigManager &getInstance()
  {
    static ConfigManager instance;
    return instance;
  }

  /**
   * @brief Initializes the ConfigManager.
   *
   * This method mounts the LittleFS filesystem and loads the configuration
   * from the JSON file. If the file doesn't exist, it creates one with
   * default values. This should be called once at startup.
   */
  void begin();

  /**
   * @brief Saves the current configuration to the JSON file on LittleFS.
   * @return True if the configuration was saved successfully, false otherwise.
   */
  bool save();

  // Getters
  /**
   * @brief Gets the stored WiFi SSID.
   * @return The WiFi SSID as a String.
   */
  String getWifiSSID() const { return wifiSSID; }

  /**
   * @brief Gets a specific alarm by its index.
   * @param index The index of the alarm.
   * @return A constant reference to the Alarm object.
   */
  const Alarm &getAlarm(int index) const;
  Alarm &getAlarm(int index);

  /**
   * @brief Gets the total number of alarms.
   * @return The number of alarms.
   */
  int getNumAlarms() const;

  /**
   * @brief Gets the stored WiFi password.
   * @return The WiFi password as a String.
   */
  String getWifiPassword() const { return wifiPassword; }

  /**
   * @brief Checks if automatic brightness control is enabled.
   * @return True if auto brightness is enabled, false otherwise.
   */
  bool isAutoBrightness() const { return autoBrightness; }

  /**
   * @brief Gets the manual brightness level.
   * @return The brightness level (0-255).
   */
  uint8_t getBrightness() const { return brightness; }

  /**
   * @brief Gets the auto brightness start hour.
   * @return The start hour (0-23).
   */
  uint8_t getAutoBrightnessStartHour() const { return autoBrightnessStartHour; }

  /**
   * @brief Gets the auto brightness end hour.
   * @return The end hour (0-23).
   */
  uint8_t getAutoBrightnessEndHour() const { return autoBrightnessEndHour; }

  /**
   * @brief Gets the day brightness level.
   * @return The brightness level (0-255).
   */
  uint8_t getDayBrightness() const { return dayBrightness; }

  /**
   * @brief Gets the night brightness level.
   * @return The brightness level (0-255).
   */
  uint8_t getNightBrightness() const { return nightBrightness; }

  /**
   * @brief Checks if the clock is in 24-hour format.
   * @return True if 24-hour format is enabled, false otherwise.
   */
  bool is24HourFormat() const { return use24HourFormat; }

  /**
   * @brief Checks if the temperature is displayed in Celsius.
   * @return True if Celsius is used, false for Fahrenheit.
   */
  bool isCelsius() const { return useCelsius; }

  /**
   * @brief Gets the custom hostname.
   * @return The hostname as a String.
   */
  String getHostname() const { return hostname; }

  /**
   * @brief Checks if the screen orientation is flipped.
   * @return True if the screen is flipped, false otherwise.
   */
  bool isScreenFlipped() const { return screenFlipped; }

  // Display Colors
  String getBackgroundColor() const { return backgroundColor; }
  String getTimeColor() const { return timeColor; }
  String getTodColor() const { return todColor; }
  String getSecondsColor() const { return secondsColor; }
  String getDayOfWeekColor() const { return dayOfWeekColor; }
  String getDateColor() const { return dateColor; }
  String getTempColor() const { return tempColor; }
  String getHumidityColor() const { return humidityColor; }
  String getAlarmIconColor() const { return alarmIconColor; }
  String getSnoozeIconColor() const { return snoozeIconColor; }
  String getAlarmTextColor() const { return alarmTextColor; }
  String getErrorTextColor() const { return errorTextColor; }

  /**
   * @brief Checks if the stored WiFi credentials have been validated.
   * @return True if the credentials are known to be good, false otherwise.
   */
  bool areWifiCredsValid() const { return wifiCredsValid; }

  // Setters
  /**
   * @brief Sets the WiFi SSID.
   * @param ssid The new WiFi SSID.
   */
  void setWifiSSID(const String &ssid)
  {
    wifiSSID = ssid;
    wifiCredsValid = false; // New credentials need validation
    _isDirty = true;
  }

  /**
   * @brief Sets the WiFi password.
   * @param password The new WiFi password.
   */
  void setWifiPassword(const String &password)
  {
    wifiPassword = password;
    wifiCredsValid = false; // New credentials need validation
    _isDirty = true;
  }

  /**
   * @brief Sets the custom hostname.
   * @param name The new hostname.
   */
  void setHostname(const String &name)
  {
    hostname = name;
    _isDirty = true;
  }

  /**
   * @brief Sets the WiFi credential validity.
   * @param valid True if the credentials are valid, false otherwise.
   */
  void setWifiCredsValid(bool valid)
  {
    if (wifiCredsValid != valid)
    {
      wifiCredsValid = valid;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the screen orientation.
   * @param flipped True to flip the screen, false for normal.
   */

  void setScreenFlipped(bool flipped)
  {
    if (screenFlipped != flipped)
    {
      screenFlipped = flipped;
      _isDirty = true;
    }
  }

  /**
   * @brief Enables or disables automatic brightness control.
   * @param enabled True to enable, false to disable.
   */
  void setAutoBrightness(bool enabled)
  {
    if (autoBrightness != enabled)
    {
      autoBrightness = enabled;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the manual brightness level.
   * @param value The brightness level (0-255).
   */
  void setBrightness(uint8_t value)
  {
    if (brightness != value)
    {
      brightness = value;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the auto brightness start hour.
   * @param value The start hour (0-23).
   */
  void setAutoBrightnessStartHour(uint8_t value)
  {
    if (autoBrightnessStartHour != value)
    {
      autoBrightnessStartHour = value;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the auto brightness end hour.
   * @param value The end hour (0-23).
   */
  void setAutoBrightnessEndHour(uint8_t value)
  {
    if (autoBrightnessEndHour != value)
    {
      autoBrightnessEndHour = value;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the day brightness level.
   * @param value The brightness level (0-255).
   */
  void setDayBrightness(uint8_t value)
  {
    if (dayBrightness != value)
    {
      dayBrightness = value;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the night brightness level.
   * @param value The brightness level (0-255).
   */
  void setNightBrightness(uint8_t value)
  {
    if (nightBrightness != value)
    {
      nightBrightness = value;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the clock to 24-hour or 12-hour format.
   * @param enabled True for 24-hour, false for 12-hour.
   */
  void set24HourFormat(bool enabled)
  {
    if (use24HourFormat != enabled)
    {
      use24HourFormat = enabled;
      _isDirty = true;
    }
  }

  /**
   * @brief Sets the temperature unit.
   * @param enabled True for Celsius, false for Fahrenheit.
   */
  void setCelsius(bool enabled)
  {
    if (useCelsius != enabled)
    {
      useCelsius = enabled;
      _isDirty = true;
    }
  }

  /**
   * @brief Updates an alarm at a specific index.
   * @param index The index of the alarm to update.
   * @param alarm The new alarm data.
   */
  void setAlarm(int index, const Alarm &alarm);

  void setBackgroundColor(const String &color)
  {
    if (backgroundColor != color)
    {
      backgroundColor = color;
      _isDirty = true;
    }
  }

  void setTimeColor(const String &color)
  {
    if (timeColor != color)
    {
      timeColor = color;
      _isDirty = true;
    }
  }

  void setTodColor(const String &color)
  {
    if (todColor != color)
    {
      todColor = color;
      _isDirty = true;
    }
  }

  void setSecondsColor(const String &color)
  {
    if (secondsColor != color)
    {
      secondsColor = color;
      _isDirty = true;
    }
  }

  void setDayOfWeekColor(const String &color)
  {
    if (dayOfWeekColor != color)
    {
      dayOfWeekColor = color;
      _isDirty = true;
    }
  }

  void setDateColor(const String &color)
  {
    if (dateColor != color)
    {
      dateColor = color;
      _isDirty = true;
    }
  }

  void setTempColor(const String &color)
  {
    if (tempColor != color)
    {
      tempColor = color;
      _isDirty = true;
    }
  }

  void setHumidityColor(const String &color)
  {
    if (humidityColor != color)
    {
      humidityColor = color;
      _isDirty = true;
    }
  }

  void setAlarmIconColor(const String &color)
  {
    if (alarmIconColor != color)
    {
      alarmIconColor = color;
      _isDirty = true;
    }
  }

  void setSnoozeIconColor(const String &color)
  {
    if (snoozeIconColor != color)
    {
      snoozeIconColor = color;
      _isDirty = true;
    }
  }

  void setAlarmTextColor(const String &color)
  {
    if (alarmTextColor != color)
    {
      alarmTextColor = color;
      _isDirty = true;
    }
  }

  void setErrorTextColor(const String &color)
  {
    if (errorTextColor != color)
    {
      errorTextColor = color;
      _isDirty = true;
    }
  }

  /**
   * @brief Checks if the configuration has been modified since the last save.
   * @return True if the configuration is "dirty", false otherwise.
   */
  bool isDirty() const { return _isDirty; }

  /**
   * @brief Clears the dirty flag, usually after handling the changes.
   */
  void clearDirtyFlag() { _isDirty = false; }

  /**
   * @brief Resets all settings to their default values.
   */
  void factoryReset();

  /**
   * @brief Resets all display settings to their default values.
   */
  void resetDisplayToDefaults();

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  ConfigManager() : _isDirty(false) {}

  // Configuration variables with default values
  String wifiSSID = DEFAULT_WIFI_SSID;
  String wifiPassword = DEFAULT_WIFI_PASSWORD;
  String hostname = DEFAULT_HOSTNAME;
  bool wifiCredsValid = DEFAULT_WIFI_CREDS_VALID;
  bool autoBrightness = DEFAULT_AUTO_BRIGHTNESS;
  uint8_t brightness = DEFAULT_BRIGHTNESS;
  uint8_t autoBrightnessStartHour = DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
  uint8_t autoBrightnessEndHour = DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
  uint8_t dayBrightness = DEFAULT_DAY_BRIGHTNESS;
  uint8_t nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
  bool use24HourFormat = DEFAULT_USE_24_HOUR_FORMAT;
  bool useCelsius = DEFAULT_USE_CELSIUS;
  bool screenFlipped = DEFAULT_SCREEN_FLIPPED;

  // Colors
  String backgroundColor = DEFAULT_BACKGROUND_COLOR;
  String timeColor = DEFAULT_TIME_COLOR;
  String todColor = DEFAULT_TOD_COLOR;
  String secondsColor = DEFAULT_SECONDS_COLOR;
  String dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  String dateColor = DEFAULT_DATE_COLOR;
  String tempColor = DEFAULT_TEMP_COLOR;
  String humidityColor = DEFAULT_HUMIDITY_COLOR;
  String alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  String snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  String alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  String errorTextColor = DEFAULT_ERROR_TEXT_COLOR;

  bool _isDirty;
  Alarm _alarms[MAX_ALARMS];
  SemaphoreHandle_t _fileMutex;

  /**
   * @brief Loads the configuration from the JSON file.
   *
   * If the file cannot be read or parsed, default values are used.
   */
  void load();

  /**
   * @brief Sets the configuration variables to their default values.
   */
  void setDefaults();
};