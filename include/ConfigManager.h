// ConfigManager.h

#pragma once

#include <Arduino.h>
#include "Alarm.h"
#include <Preferences.h>
#include <vector>

const int LEGACY_ALARMS_COUNT = 5;  // Default number of alarms for legacy data migration
const int DEFAULT_ALARMS_COUNT = 2; // Default number of alarms for new installs/resets
const int MAX_ALLOWED_ALARMS = 20;  // Maximum number of alarms allowed

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
static constexpr int8_t DEFAULT_RINGING_ALARM_ID = -1;
static constexpr uint32_t DEFAULT_RINGING_ALARM_TIMESTAMP = 0;
static constexpr bool DEFAULT_AUTO_BRIGHTNESS = true;
static constexpr uint8_t DEFAULT_BRIGHTNESS = 128;
static constexpr uint8_t DEFAULT_AUTO_BRIGHTNESS_START_HOUR = 7;
static constexpr uint8_t DEFAULT_AUTO_BRIGHTNESS_END_HOUR = 21;
static constexpr uint8_t DEFAULT_DAY_BRIGHTNESS = 255;
static constexpr uint8_t DEFAULT_NIGHT_BRIGHTNESS = 10;
static constexpr bool DEFAULT_USE_24_HOUR_FORMAT = false;
static constexpr bool DEFAULT_USE_CELSIUS = false;
static constexpr bool DEFAULT_SCREEN_FLIPPED = false;
static constexpr bool DEFAULT_INVERT_COLORS = false;
static constexpr const char *DEFAULT_TIMEZONE = "EST5EDT,M3.2.0/2:00,M11.1.0/2:00";
static constexpr bool DEFAULT_TEMP_CORRECTION_ENABLED = true;
static constexpr float DEFAULT_TEMP_CORRECTION = 0.0;
static constexpr bool DEFAULT_IS_DST = false;
static constexpr uint8_t DEFAULT_SNOOZE_DURATION = 9;
static constexpr uint8_t DEFAULT_DISMISS_DURATION = 3;

/**
 * @class ConfigManager
 * @brief Manages the application's configuration settings using a singleton pattern.
 *
 * This class is responsible for loading, saving, and providing access to
 * configuration settings such as WiFi credentials, display preferences, and
 * other system options. Settings are stored in a preferences object.
 */
class ConfigManager
{
public:
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
   * @brief Handles periodic tasks for the ConfigManager, like debounced saves.
   *
   * This should be called in the main application loop. It checks if a save
   * operation is pending and executes it after a debounce delay.
   */
  void loop();

  /**
   * @brief Saves the current configuration to the JSON file.
   * @return True if the save was successful, false otherwise.
   */
  bool save();

  /**
   * @brief Schedules a save operation.
   *
   * This method sets a flag to save the configuration after a short delay.
   * This is used to prevent rapid, successive writes to the filesystem when
   * multiple settings are changed in quick succession.
   */
  void scheduleSave();

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
  const Alarm &getAlarmByIndex(int index) const;
  Alarm &getAlarmByIndex(int index);

  /**
   * @brief Gets a specific alarm by its unique ID.
   * @param id The unique ID of the alarm.
   * @return A pointer to the Alarm object, or nullptr if not found.
   */
  Alarm *getAlarmById(int id);
  const Alarm *getAlarmById(int id) const;

  /**
   * @brief Gets the total number of alarms.
   * @return The number of alarms.
   */
  int getNumAlarms() const;

  /**
   * @brief Replaces the current list of alarms with a new one.
   * Assigns new IDs to alarms with ID -1.
   * @param newAlarms The new vector of alarms.
   */
  void replaceAlarms(const std::vector<Alarm> &newAlarms);

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

  /**
   * @brief Checks if the screen colors are inverted.
   * @return True if the screen is inverted, false otherwise.
   */
  bool isInvertColors() const { return invertColors; }

  /**
   * @brief Gets the timezone string.
   * @return The timezone string.
   */
  String getTimezone() const { return timezone; }

  /**
   * @brief Gets the snooze duration for alarms.
   * @return The snooze duration in minutes.
   */
  uint8_t getSnoozeDuration() const { return snoozeDuration; }

  /**
   * @brief Gets the hold duration to dismiss an alarm.
   * @return The dismiss duration in seconds.
   */
  uint8_t getDismissDuration() const { return dismissDuration; }

  /**
   * @brief Gets the temperature correction value.
   * @return The correction value.
   */
  float getTempCorrection() const { return tempCorrection; }

  /**
   * @brief Checks if temperature correction is enabled.
   * @return True if enabled, false otherwise.
   */
  bool isTempCorrectionEnabled() const { return tempCorrectionEnabled; }

  /**
   * @brief Checks if Daylight Saving Time is currently active.
   * @return True if DST is active, false otherwise.
   */
  bool isDST() const { return isDst; }

  // Display Colors

  /**
   * @brief Gets the background color of the display.
   * @return The color as a hex string (e.g., "#RRGGBB").
   */
  String getBackgroundColor() const { return backgroundColor; }

  /**
   * @brief Gets the color of the time display.
   * @return The color as a hex string.
   */
  String getTimeColor() const { return timeColor; }

  /**
   * @brief Gets the color of the AM/PM indicator.
   * @return The color as a hex string.
   */
  String getTodColor() const { return todColor; }

  /**
   * @brief Gets the color of the seconds display.
   * @return The color as a hex string.
   */
  String getSecondsColor() const { return secondsColor; }

  /**
   * @brief Gets the color of the day of the week display.
   * @return The color as a hex string.
   */
  String getDayOfWeekColor() const { return dayOfWeekColor; }

  /**
   * @brief Gets the color of the date display.
   * @return The color as a hex string.
   */
  String getDateColor() const { return dateColor; }

  /**
   * @brief Gets the color of the temperature display.
   * @return The color as a hex string.
   */
  String getTempColor() const { return tempColor; }

  /**
   * @brief Gets the color of the humidity display.
   * @return The color as a hex string.
   */
  String getHumidityColor() const { return humidityColor; }

  /**
   * @brief Gets the color of the alarm icon.
   * @return The color as a hex string.
   */
  String getAlarmIconColor() const { return alarmIconColor; }

  /**
   * @brief Gets the color of the snooze icon.
   * @return The color as a hex string.
   */
  String getSnoozeIconColor() const { return snoozeIconColor; }

  /**
   * @brief Gets the color of the alarm text when ringing.
   * @return The color as a hex string.
   */
  String getAlarmTextColor() const { return alarmTextColor; }

  /**
   * @brief Gets the color of error text messages.
   * @return The color as a hex string.
   */
  String getErrorTextColor() const { return errorTextColor; }

  /**
   * @brief Gets the ID of the alarm that was ringing at shutdown.
   * @return The ID of the alarm, or -1 if none.
   */
  int8_t getRingingAlarmId() const { return ringingAlarmId; }

  /**
   * @brief Gets the Unix timestamp of when the ringing alarm started.
   * @return The start timestamp, or 0 if no alarm was ringing.
   */
  uint32_t getRingingAlarmStartTimestamp() const { return ringingAlarmStartTimestamp; }

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
    scheduleSave();
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
    scheduleSave();
  }

  /**
   * @brief Sets the custom hostname.
   * @param name The new hostname.
   */
  void setHostname(const String &name)
  {
    hostname = name;
    _isDirty = true;
    scheduleSave();
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
      scheduleSave();
    }
  }

  /**
   * @brief Enables or disables temperature correction.
   * @param enabled True to enable, false to disable.
   */
  void setTempCorrectionEnabled(bool enabled)
  {
    if (tempCorrectionEnabled != enabled)
    {
      tempCorrectionEnabled = enabled;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the temperature correction value.
   * @param value The new correction value.
   */
  void setTempCorrection(float value)
  {
    if (tempCorrection != value)
    {
      tempCorrection = value;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the Daylight Saving Time status.
   * @param active True if DST is active, false otherwise.
   */
  void setDST(bool active)
  {
    if (isDst != active)
    {
      isDst = active;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the screen color inversion.
   * @param inverted True to invert the screen, false for normal.
   */
  void setInvertColors(bool inverted)
  {
    if (invertColors != inverted)
    {
      invertColors = inverted;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the snooze duration for alarms.
   * @param duration The snooze duration in minutes.
   */
  void setSnoozeDuration(uint8_t duration)
  {
    if (snoozeDuration != duration)
    {
      snoozeDuration = duration;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the hold duration to dismiss an alarm.
   * @param duration The dismiss duration in seconds.
   */
  void setDismissDuration(uint8_t duration)
  {
    if (dismissDuration != duration)
    {
      dismissDuration = duration;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the ID of the currently ringing alarm.
   * @param id The alarm ID, or -1 to clear.
   */
  void setRingingAlarmId(int8_t id)
  {
    if (ringingAlarmId != id)
    {
      ringingAlarmId = id;
    }
  }

  /**
   * @brief Sets the start timestamp of the ringing alarm.
   * @param timestamp The Unix timestamp.
   */
  void setRingingAlarmStartTimestamp(uint32_t timestamp)
  {
    if (ringingAlarmStartTimestamp != timestamp)
    {
      ringingAlarmStartTimestamp = timestamp;
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
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
      scheduleSave();
    }
  }

  /**
   * @brief Sets the timezone string.
   * @param tz The new timezone string.
   */
  void setTimezone(const String &tz)
  {
    if (timezone != tz)
    {
      timezone = tz;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Updates an alarm at a specific index.
   * @param index The index of the alarm to update.
   * @param alarm The new alarm data.
   */
  void setAlarmByIndex(int index, const Alarm &alarm);

  /**
   * @brief Updates an alarm by its unique ID.
   * @param id The unique ID of the alarm.
   * @param alarm The new alarm data.
   */
  void setAlarmById(int id, const Alarm &alarm);

  /**
   * @brief Sets the background color of the display.
   * @param color The new color as a hex string (e.g., "#RRGGBB").
   */
  void setBackgroundColor(const String &color)
  {
    if (backgroundColor != color)
    {
      backgroundColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the time display.
   * @param color The new color as a hex string.
   */
  void setTimeColor(const String &color)
  {
    if (timeColor != color)
    {
      timeColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the AM/PM indicator.
   * @param color The new color as a hex string.
   */
  void setTodColor(const String &color)
  {
    if (todColor != color)
    {
      todColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the seconds display.
   * @param color The new color as a hex string.
   */
  void setSecondsColor(const String &color)
  {
    if (secondsColor != color)
    {
      secondsColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the day of the week display.
   * @param color The new color as a hex string.
   */
  void setDayOfWeekColor(const String &color)
  {
    if (dayOfWeekColor != color)
    {
      dayOfWeekColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the date display.
   * @param color The new color as a hex string.
   */
  void setDateColor(const String &color)
  {
    if (dateColor != color)
    {
      dateColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the temperature display.
   * @param color The new color as a hex string.
   */
  void setTempColor(const String &color)
  {
    if (tempColor != color)
    {
      tempColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the humidity display.
   * @param color The new color as a hex string.
   */
  void setHumidityColor(const String &color)
  {
    if (humidityColor != color)
    {
      humidityColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the alarm icon.
   * @param color The new color as a hex string.
   */
  void setAlarmIconColor(const String &color)
  {
    if (alarmIconColor != color)
    {
      alarmIconColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the snooze icon.
   * @param color The new color as a hex string.
   */
  void setSnoozeIconColor(const String &color)
  {
    if (snoozeIconColor != color)
    {
      snoozeIconColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of the alarm text when ringing.
   * @param color The new color as a hex string.
   */
  void setAlarmTextColor(const String &color)
  {
    if (alarmTextColor != color)
    {
      alarmTextColor = color;
      _isDirty = true;
      scheduleSave();
    }
  }

  /**
   * @brief Sets the color of error text messages.
   * @param color The new color as a hex string.
   */
  void setErrorTextColor(const String &color)
  {
    if (errorTextColor != color)
    {
      errorTextColor = color;
      _isDirty = true;
      scheduleSave();
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
   * @brief Saves the state of a ringing alarm to persistent storage.
   *
   * This is used to resume an alarm if the device reboots while it's ringing.
   */
  void saveRingingAlarmState();

  /**
   * @brief Resets all settings to their default values.
   */
  void factoryReset();

  /**
   * @brief Resets all settings to their default values, except for WiFi credentials.
   */
  void factoryResetExceptWiFi();

  /**
   * @brief Resets all display settings to their default values.
   */
  void resetDisplayToDefaults();

  /**
   * @brief Resets all general settings to their default values.
   */
  void resetGeneralSettingsToDefaults();

  bool isAnyAlarmSnoozed() const;

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  ConfigManager() : _isDirty(false), _savePending(false), _saveDebounceTimer(0), _nextAlarmId(0) {}

  // Configuration variables with default values
  String wifiSSID = DEFAULT_WIFI_SSID;
  String wifiPassword = DEFAULT_WIFI_PASSWORD;
  String hostname = DEFAULT_HOSTNAME;
  bool wifiCredsValid = DEFAULT_WIFI_CREDS_VALID;
  int8_t ringingAlarmId = DEFAULT_RINGING_ALARM_ID;
  uint32_t ringingAlarmStartTimestamp = DEFAULT_RINGING_ALARM_TIMESTAMP;
  bool autoBrightness = DEFAULT_AUTO_BRIGHTNESS;
  uint8_t brightness = DEFAULT_BRIGHTNESS;
  uint8_t autoBrightnessStartHour = DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
  uint8_t autoBrightnessEndHour = DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
  uint8_t dayBrightness = DEFAULT_DAY_BRIGHTNESS;
  uint8_t nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
  bool use24HourFormat = DEFAULT_USE_24_HOUR_FORMAT;
  bool useCelsius = DEFAULT_USE_CELSIUS;
  bool screenFlipped = DEFAULT_SCREEN_FLIPPED;
  bool invertColors = DEFAULT_INVERT_COLORS;
  String timezone = DEFAULT_TIMEZONE;
  bool tempCorrectionEnabled = DEFAULT_TEMP_CORRECTION_ENABLED;
  float tempCorrection = DEFAULT_TEMP_CORRECTION;
  bool isDst = DEFAULT_IS_DST;
  uint8_t snoozeDuration = DEFAULT_SNOOZE_DURATION;
  uint8_t dismissDuration = DEFAULT_DISMISS_DURATION;

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
  bool _savePending;
  unsigned long _saveDebounceTimer;
  std::vector<Alarm> _alarms;
  int _nextAlarmId;
  Preferences _preferences;

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
