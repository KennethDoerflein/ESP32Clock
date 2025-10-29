// ConfigManager.cpp

#include "ConfigManager.h"
#include "SerialLog.h"
#include "nvs_flash.h"

// Define the namespace for preferences
#define PREFERENCES_NAMESPACE "clock_config"

void ConfigManager::begin()
{
  _preferences.begin(PREFERENCES_NAMESPACE, false); // R/W mode
  // After ensuring the filesystem is ready, load the configuration.
  load();
}

void ConfigManager::setDefaults()
{
  // Set all configuration parameters to their default initial state.
  wifiSSID = DEFAULT_WIFI_SSID;
  wifiPassword = DEFAULT_WIFI_PASSWORD;
  hostname = DEFAULT_HOSTNAME;
  wifiCredsValid = DEFAULT_WIFI_CREDS_VALID;
  ringingAlarmId = DEFAULT_RINGING_ALARM_ID;
  ringingAlarmStartTimestamp = DEFAULT_RINGING_ALARM_TIMESTAMP;
  autoBrightness = DEFAULT_AUTO_BRIGHTNESS;
  brightness = DEFAULT_BRIGHTNESS;
  autoBrightnessStartHour = DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
  autoBrightnessEndHour = DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
  dayBrightness = DEFAULT_DAY_BRIGHTNESS;
  nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
  use24HourFormat = DEFAULT_USE_24_HOUR_FORMAT;
  useCelsius = DEFAULT_USE_CELSIUS;
  screenFlipped = DEFAULT_SCREEN_FLIPPED;
  timezone = DEFAULT_TIMEZONE;
  snoozeDuration = DEFAULT_SNOOZE_DURATION;
  dismissDuration = DEFAULT_DISMISS_DURATION;

  backgroundColor = DEFAULT_BACKGROUND_COLOR;
  timeColor = DEFAULT_TIME_COLOR;
  todColor = DEFAULT_TOD_COLOR;
  secondsColor = DEFAULT_SECONDS_COLOR;
  dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  dateColor = DEFAULT_DATE_COLOR;
  tempColor = DEFAULT_TEMP_COLOR;
  humidityColor = DEFAULT_HUMIDITY_COLOR;
  alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  errorTextColor = DEFAULT_ERROR_TEXT_COLOR;

  for (int i = 0; i < MAX_ALARMS; ++i)
  {
    _alarms[i] = Alarm(); // Reset to default constructor
    _alarms[i].setId(i);
  }

  SerialLog::getInstance().print("Loaded default configuration.");
}

void ConfigManager::load()
{
  // Check if this is the first boot
  bool firstBoot = !_preferences.getBool("first_boot_done", false);
  if (firstBoot)
  {
    SerialLog::getInstance().print("First boot detected. Loading default configuration.");
    setDefaults();
    save(); // Save defaults to preferences
    _preferences.putBool("first_boot_done", true);
    return;
  }

  SerialLog::getInstance().print("Loading configuration from Preferences...");

  // Load each setting from the Preferences
  wifiSSID = _preferences.getString("wifiSSID", DEFAULT_WIFI_SSID);
  wifiPassword = _preferences.getString("wifiPassword", DEFAULT_WIFI_PASSWORD);
  hostname = _preferences.getString("hostname", DEFAULT_HOSTNAME);
  wifiCredsValid = _preferences.getBool("wifiCredsValid", DEFAULT_WIFI_CREDS_VALID);
  ringingAlarmId = _preferences.getChar("ringingAlarmId", DEFAULT_RINGING_ALARM_ID);
  ringingAlarmStartTimestamp = _preferences.getUInt("ringingAlarmStart", DEFAULT_RINGING_ALARM_TIMESTAMP);
  autoBrightness = _preferences.getBool("autoBrightness", DEFAULT_AUTO_BRIGHTNESS);
  brightness = _preferences.getUChar("brightness", DEFAULT_BRIGHTNESS);
  autoBrightnessStartHour = _preferences.getUChar("autoBrightStart", DEFAULT_AUTO_BRIGHTNESS_START_HOUR);
  autoBrightnessEndHour = _preferences.getUChar("autoBrightEnd", DEFAULT_AUTO_BRIGHTNESS_END_HOUR);
  dayBrightness = _preferences.getUChar("dayBrightness", DEFAULT_DAY_BRIGHTNESS);
  nightBrightness = _preferences.getUChar("nightBrightness", DEFAULT_NIGHT_BRIGHTNESS);
  use24HourFormat = _preferences.getBool("use24HourFormat", DEFAULT_USE_24_HOUR_FORMAT);
  useCelsius = _preferences.getBool("useCelsius", DEFAULT_USE_CELSIUS);
  screenFlipped = _preferences.getBool("screenFlipped", DEFAULT_SCREEN_FLIPPED);
  timezone = _preferences.getString("timezone", DEFAULT_TIMEZONE);
  snoozeDuration = _preferences.getUChar("snoozeDuration", DEFAULT_SNOOZE_DURATION);
  dismissDuration = _preferences.getUChar("dismissDuration", DEFAULT_DISMISS_DURATION);

  backgroundColor = _preferences.getString("backgroundColor", DEFAULT_BACKGROUND_COLOR);
  timeColor = _preferences.getString("timeColor", DEFAULT_TIME_COLOR);
  todColor = _preferences.getString("todColor", DEFAULT_TOD_COLOR);
  secondsColor = _preferences.getString("secondsColor", DEFAULT_SECONDS_COLOR);
  dayOfWeekColor = _preferences.getString("dayOfWeekColor", DEFAULT_DAY_OF_WEEK_COLOR);
  dateColor = _preferences.getString("dateColor", DEFAULT_DATE_COLOR);
  tempColor = _preferences.getString("tempColor", DEFAULT_TEMP_COLOR);
  humidityColor = _preferences.getString("humidityColor", DEFAULT_HUMIDITY_COLOR);
  alarmIconColor = _preferences.getString("alarmIconColor", DEFAULT_ALARM_ICON_COLOR);
  snoozeIconColor = _preferences.getString("snoozeIconColor", DEFAULT_SNOOZE_ICON_COLOR);
  alarmTextColor = _preferences.getString("alarmTextColor", DEFAULT_ALARM_TEXT_COLOR);
  errorTextColor = _preferences.getString("errorTextColor", DEFAULT_ERROR_TEXT_COLOR);

  // Validate colors to prevent issues with URL-encoded values
  if (backgroundColor.startsWith("%"))
    backgroundColor = DEFAULT_BACKGROUND_COLOR;
  if (timeColor.startsWith("%"))
    timeColor = DEFAULT_TIME_COLOR;
  if (todColor.startsWith("%"))
    todColor = DEFAULT_TOD_COLOR;
  if (secondsColor.startsWith("%"))
    secondsColor = DEFAULT_SECONDS_COLOR;
  if (dayOfWeekColor.startsWith("%"))
    dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  if (dateColor.startsWith("%"))
    dateColor = DEFAULT_DATE_COLOR;
  if (tempColor.startsWith("%"))
    tempColor = DEFAULT_TEMP_COLOR;
  if (humidityColor.startsWith("%"))
    humidityColor = DEFAULT_HUMIDITY_COLOR;
  if (alarmIconColor.startsWith("%"))
    alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  if (snoozeIconColor.startsWith("%"))
    snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  if (alarmTextColor.startsWith("%"))
    alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  if (errorTextColor.startsWith("%"))
    errorTextColor = DEFAULT_ERROR_TEXT_COLOR;

  // Load alarms
  for (int i = 0; i < MAX_ALARMS; ++i)
  {
    String prefix = "alarm_" + String(i) + "_";
    _alarms[i].setId(i);
    _alarms[i].setEnabled(_preferences.getBool((prefix + "enabled").c_str(), false));
    _alarms[i].setHour(_preferences.getUChar((prefix + "hour").c_str(), 6));
    _alarms[i].setMinute(_preferences.getUChar((prefix + "minute").c_str(), 0));
    _alarms[i].setDays(_preferences.getUChar((prefix + "days").c_str(), 0));
    bool snoozed = _preferences.getBool((prefix + "snoozed").c_str(), false);
    uint32_t snoozeUntil = _preferences.getUInt((prefix + "snoozeUntil").c_str(), 0);
    _alarms[i].setSnoozeState(snoozed, snoozeUntil);
    _alarms[i].setLastDismissedDay(_preferences.getUChar((prefix + "lastDismissed").c_str(), 8));
  }

  SerialLog::getInstance().print("Configuration loaded successfully.");
}

bool ConfigManager::save()
{
  _preferences.putString("wifiSSID", wifiSSID);
  _preferences.putString("wifiPassword", wifiPassword);
  _preferences.putString("hostname", hostname);
  _preferences.putBool("wifiCredsValid", wifiCredsValid);
  _preferences.putChar("ringingAlarmId", ringingAlarmId);
  _preferences.putUInt("ringingAlarmStart", ringingAlarmStartTimestamp);
  _preferences.putBool("autoBrightness", autoBrightness);
  _preferences.putUChar("brightness", brightness);
  _preferences.putUChar("autoBrightStart", autoBrightnessStartHour);
  _preferences.putUChar("autoBrightEnd", autoBrightnessEndHour);
  _preferences.putUChar("dayBrightness", dayBrightness);
  _preferences.putUChar("nightBrightness", nightBrightness);
  _preferences.putBool("use24HourFormat", use24HourFormat);
  _preferences.putBool("useCelsius", useCelsius);
  _preferences.putBool("screenFlipped", screenFlipped);
  _preferences.putString("timezone", timezone);
  _preferences.putUChar("snoozeDuration", snoozeDuration);
  _preferences.putUChar("dismissDuration", dismissDuration);

  _preferences.putString("backgroundColor", backgroundColor);
  _preferences.putString("timeColor", timeColor);
  _preferences.putString("todColor", todColor);
  _preferences.putString("secondsColor", secondsColor);
  _preferences.putString("dayOfWeekColor", dayOfWeekColor);
  _preferences.putString("dateColor", dateColor);
  _preferences.putString("tempColor", tempColor);
  _preferences.putString("humidityColor", humidityColor);
  _preferences.putString("alarmIconColor", alarmIconColor);
  _preferences.putString("snoozeIconColor", snoozeIconColor);
  _preferences.putString("alarmTextColor", alarmTextColor);
  _preferences.putString("errorTextColor", errorTextColor);

  // Save alarms
  for (int i = 0; i < MAX_ALARMS; ++i)
  {
    String prefix = "alarm_" + String(i) + "_";
    _preferences.putBool((prefix + "enabled").c_str(), _alarms[i].isEnabled());
    _preferences.putUChar((prefix + "hour").c_str(), _alarms[i].getHour());
    _preferences.putUChar((prefix + "minute").c_str(), _alarms[i].getMinute());
    _preferences.putUChar((prefix + "days").c_str(), _alarms[i].getDays());
    _preferences.putBool((prefix + "snoozed").c_str(), _alarms[i].isSnoozed());
    _preferences.putUInt((prefix + "snoozeUntil").c_str(), _alarms[i].getSnoozeUntil());
    _preferences.putUChar((prefix + "lastDismissed").c_str(), _alarms[i].getLastDismissedDay());
  }

  SerialLog::getInstance().print("Configuration saved.");
  return true;
}

const Alarm &ConfigManager::getAlarm(int index) const
{
  if (index < 0 || index >= MAX_ALARMS)
  {
    // This is an error, restart the system.
    SerialLog::getInstance().print("FATAL: Alarm index out of bounds!");
    ESP.restart(); // Restart the ESP32 to recover
  }
  return _alarms[index];
}

Alarm &ConfigManager::getAlarm(int index)
{
  if (index < 0 || index >= MAX_ALARMS)
  {
    // This is an error, restart the system.
    SerialLog::getInstance().print("FATAL: Alarm index out of bounds!");
    ESP.restart(); // Restart the ESP32 to recover
  }
  return _alarms[index];
}

int ConfigManager::getNumAlarms() const
{
  return MAX_ALARMS;
}

void ConfigManager::setAlarm(int index, const Alarm &alarm)
{
  if (index < 0 || index >= MAX_ALARMS)
  {
    SerialLog::getInstance().print("ERROR: Alarm index out of bounds!");
    return;
  }
  _alarms[index] = alarm;
  _isDirty = true;
}

void ConfigManager::factoryReset()
{
  SerialLog::getInstance().print("Performing factory reset...\n");

  // Erase the NVS (Non-Volatile Storage) partition.
  // This is where the WiFi credentials are saved by the WiFi library.
  SerialLog::getInstance().print("Erasing NVS to clear WiFi credentials...\n");
  esp_err_t err = nvs_flash_erase();
  if (err == ESP_OK)
  {
    SerialLog::getInstance().print("NVS erased successfully.\n");
  }
  else
  {
    SerialLog::getInstance().print("Error erasing NVS.\n");
  }

  // After erasing, the NVS needs to be re-initialized for the next boot.
  err = nvs_flash_init();
  if (err == ESP_OK)
  {
    SerialLog::getInstance().print("NVS re-initialized successfully.\n");
  }
  else
  {
    SerialLog::getInstance().print("Error re-initializing NVS.\n");
  }

  setDefaults();
  save();
}

void ConfigManager::factoryResetExceptWiFi()
{
  SerialLog::getInstance().print("Performing factory reset, but keeping WiFi credentials...\n");

  // Preserve WiFi credentials
  String ssid = wifiSSID;
  String password = wifiPassword;
  bool credsValid = wifiCredsValid;

  setDefaults();

  // Restore WiFi credentials
  wifiSSID = ssid;
  wifiPassword = password;
  wifiCredsValid = credsValid;

  save();
}

void ConfigManager::resetDisplayToDefaults()
{
  backgroundColor = DEFAULT_BACKGROUND_COLOR;
  timeColor = DEFAULT_TIME_COLOR;
  todColor = DEFAULT_TOD_COLOR;
  secondsColor = DEFAULT_SECONDS_COLOR;
  dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  dateColor = DEFAULT_DATE_COLOR;
  tempColor = DEFAULT_TEMP_COLOR;
  humidityColor = DEFAULT_HUMIDITY_COLOR;
  alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  errorTextColor = DEFAULT_ERROR_TEXT_COLOR;
  _isDirty = true;
}

void ConfigManager::resetGeneralSettingsToDefaults()
{
  autoBrightness = DEFAULT_AUTO_BRIGHTNESS;
  brightness = DEFAULT_BRIGHTNESS;
  autoBrightnessStartHour = DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
  autoBrightnessEndHour = DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
  dayBrightness = DEFAULT_DAY_BRIGHTNESS;
  nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
  use24HourFormat = DEFAULT_USE_24_HOUR_FORMAT;
  useCelsius = DEFAULT_USE_CELSIUS;
  screenFlipped = DEFAULT_SCREEN_FLIPPED;
  timezone = DEFAULT_TIMEZONE;
  snoozeDuration = DEFAULT_SNOOZE_DURATION;
  dismissDuration = DEFAULT_DISMISS_DURATION;
  _isDirty = true;
}
