// ConfigManager.cpp

#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "SerialLog.h"
#include "nvs_flash.h"

// Define the path for the configuration file on the LittleFS filesystem.
#define CONFIG_FILE "/config.json"

void ConfigManager::begin()
{
  _fileMutex = xSemaphoreCreateMutex();
  // Initialize LittleFS. The `true` parameter formats the filesystem if mounting fails.
  // This is crucial for the first boot or if the filesystem becomes corrupted.
  if (!LittleFS.begin(true))
  {

    SerialLog::getInstance().print("An error occurred while mounting LittleFS. Halting.");
    // Halt execution if the filesystem cannot be prepared, as configuration is critical.
    while (1)
      ; // Infinite loop to stop the device
  }
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
  // Check if the configuration file exists.
  if (!LittleFS.exists(CONFIG_FILE))
  {
    SerialLog::getInstance().print("Config file not found, creating with defaults.");
    // If it doesn't exist, apply default settings and save them to a new file.
    setDefaults();
    save();
    return;
  }

  // Open the configuration file for reading.
  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile)
  {
    SerialLog::getInstance().print("Failed to open config file for reading. Using defaults.");
    // If the file can't be opened, fall back to default settings.
    setDefaults();
    return;
  }

  // Create a JSON document to hold the configuration data.
  // The size is determined by ArduinoJson's recommendation algorithm.
  JsonDocument doc;
  // Parse the JSON data from the file.
  DeserializationError error = deserializeJson(doc, configFile);
  // Close the file as soon as it's no longer needed.
  configFile.close();

  // Check for parsing errors.
  if (error)
  {
    Serial.print("Failed to parse config file: ");
    SerialLog::getInstance().print(error.c_str());
    // If parsing fails, the file is likely corrupt. Use default settings.
    setDefaults();
    return;
  }

  // Load each setting from the JSON document.
  // The `|` operator provides a default value if the key is missing.
  wifiSSID = doc["wifiSSID"] | DEFAULT_WIFI_SSID;
  wifiPassword = doc["wifiPassword"] | DEFAULT_WIFI_PASSWORD;
  hostname = doc["hostname"] | DEFAULT_HOSTNAME;
  wifiCredsValid = doc["wifiCredsValid"] | DEFAULT_WIFI_CREDS_VALID;
  ringingAlarmId = doc["ringingAlarmId"] | DEFAULT_RINGING_ALARM_ID;
  ringingAlarmStartTimestamp = doc["ringingAlarmStartTimestamp"] | DEFAULT_RINGING_ALARM_TIMESTAMP;
  autoBrightness = doc["autoBrightness"] | DEFAULT_AUTO_BRIGHTNESS;
  brightness = doc["brightness"] | DEFAULT_BRIGHTNESS;
  autoBrightnessStartHour = doc["autoBrightnessStartHour"] | DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
  autoBrightnessEndHour = doc["autoBrightnessEndHour"] | DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
  dayBrightness = doc["dayBrightness"] | DEFAULT_DAY_BRIGHTNESS;
  nightBrightness = doc["nightBrightness"] | DEFAULT_NIGHT_BRIGHTNESS;
  use24HourFormat = doc["use24HourFormat"] | DEFAULT_USE_24_HOUR_FORMAT;
  useCelsius = doc["useCelsius"] | DEFAULT_USE_CELSIUS;
  screenFlipped = doc["screenFlipped"] | DEFAULT_SCREEN_FLIPPED;

  backgroundColor = doc["backgroundColor"] | DEFAULT_BACKGROUND_COLOR;
  if (backgroundColor.startsWith("%"))
    backgroundColor = DEFAULT_BACKGROUND_COLOR;
  timeColor = doc["timeColor"] | DEFAULT_TIME_COLOR;
  if (timeColor.startsWith("%"))
    timeColor = DEFAULT_TIME_COLOR;
  todColor = doc["todColor"] | DEFAULT_TOD_COLOR;
  if (todColor.startsWith("%"))
    todColor = DEFAULT_TOD_COLOR;
  secondsColor = doc["secondsColor"] | DEFAULT_SECONDS_COLOR;
  if (secondsColor.startsWith("%"))
    secondsColor = DEFAULT_SECONDS_COLOR;
  dayOfWeekColor = doc["dayOfWeekColor"] | DEFAULT_DAY_OF_WEEK_COLOR;
  if (dayOfWeekColor.startsWith("%"))
    dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  dateColor = doc["dateColor"] | DEFAULT_DATE_COLOR;
  if (dateColor.startsWith("%"))
    dateColor = DEFAULT_DATE_COLOR;
  tempColor = doc["tempColor"] | DEFAULT_TEMP_COLOR;
  if (tempColor.startsWith("%"))
    tempColor = DEFAULT_TEMP_COLOR;
  humidityColor = doc["humidityColor"] | DEFAULT_HUMIDITY_COLOR;
  if (humidityColor.startsWith("%"))
    humidityColor = DEFAULT_HUMIDITY_COLOR;
  alarmIconColor = doc["alarmIconColor"] | DEFAULT_ALARM_ICON_COLOR;
  if (alarmIconColor.startsWith("%"))
    alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  snoozeIconColor = doc["snoozeIconColor"] | DEFAULT_SNOOZE_ICON_COLOR;
  if (snoozeIconColor.startsWith("%"))
    snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  alarmTextColor = doc["alarmTextColor"] | DEFAULT_ALARM_TEXT_COLOR;
  if (alarmTextColor.startsWith("%"))
    alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  errorTextColor = doc["errorTextColor"] | DEFAULT_ERROR_TEXT_COLOR;
  if (errorTextColor.startsWith("%"))
    errorTextColor = DEFAULT_ERROR_TEXT_COLOR;

  // Deserialize the alarms array
  JsonArray alarmsArray = doc["alarms"];
  if (!alarmsArray.isNull())
  {
    int i = 0;
    for (JsonObject alarmObj : alarmsArray)
    {
      if (i >= MAX_ALARMS)
        break;
      _alarms[i].setId(alarmObj["id"] | i);
      _alarms[i].setEnabled(alarmObj["enabled"] | false);
      _alarms[i].setHour(alarmObj["hour"] | 6);
      _alarms[i].setMinute(alarmObj["minute"] | 0);
      _alarms[i].setDays(alarmObj["days"] | 0);
      _alarms[i].setSnoozeState(alarmObj["snoozed"] | false, alarmObj["snoozeUntil"] | 0);
      _alarms[i].setLastDismissedDay(alarmObj["lastDismissedDay"] | 8);
      i++;
    }
  }

  SerialLog::getInstance().print("Configuration loaded successfully.");
}

bool ConfigManager::save()
{
  // Take the mutex to ensure exclusive access to the file system.
  xSemaphoreTake(_fileMutex, portMAX_DELAY);

  // Open the configuration file in write mode, which creates/truncates it.
  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile)
  {
    SerialLog::getInstance().print("Failed to open config file for writing.");
    xSemaphoreGive(_fileMutex); // Release the mutex before returning.
    return false;
  }

  // Create a JSON document to store the current settings.
  JsonDocument doc;
  doc["wifiSSID"] = wifiSSID;
  doc["wifiPassword"] = wifiPassword;
  doc["hostname"] = hostname;
  doc["wifiCredsValid"] = wifiCredsValid;
  doc["ringingAlarmId"] = ringingAlarmId;
  doc["ringingAlarmStartTimestamp"] = ringingAlarmStartTimestamp;
  doc["autoBrightness"] = autoBrightness;
  doc["brightness"] = brightness;
  doc["autoBrightnessStartHour"] = autoBrightnessStartHour;
  doc["autoBrightnessEndHour"] = autoBrightnessEndHour;
  doc["dayBrightness"] = dayBrightness;
  doc["nightBrightness"] = nightBrightness;
  doc["use24HourFormat"] = use24HourFormat;
  doc["useCelsius"] = useCelsius;
  doc["screenFlipped"] = screenFlipped;

  doc["backgroundColor"] = backgroundColor;
  doc["timeColor"] = timeColor;
  doc["todColor"] = todColor;
  doc["secondsColor"] = secondsColor;
  doc["dayOfWeekColor"] = dayOfWeekColor;
  doc["dateColor"] = dateColor;
  doc["tempColor"] = tempColor;
  doc["humidityColor"] = humidityColor;
  doc["alarmIconColor"] = alarmIconColor;
  doc["snoozeIconColor"] = snoozeIconColor;
  doc["alarmTextColor"] = alarmTextColor;
  doc["errorTextColor"] = errorTextColor;

  // Serialize the alarms array
  JsonArray alarmsArray = doc["alarms"].to<JsonArray>();
  for (int i = 0; i < MAX_ALARMS; ++i)
  {
    JsonObject alarmObj = alarmsArray.add<JsonObject>();
    alarmObj["id"] = _alarms[i].getId();
    alarmObj["enabled"] = _alarms[i].isEnabled();
    alarmObj["hour"] = _alarms[i].getHour();
    alarmObj["minute"] = _alarms[i].getMinute();
    alarmObj["days"] = _alarms[i].getDays();
    alarmObj["snoozed"] = _alarms[i].isSnoozed();
    alarmObj["snoozeUntil"] = _alarms[i].getSnoozeUntil();
    alarmObj["lastDismissedDay"] = _alarms[i].getLastDismissedDay();
  }

  // Serialize the JSON document into the file.
  if (serializeJson(doc, configFile) == 0)
  {
    SerialLog::getInstance().print("Failed to write to config file.");
    // Close the file and report the failure.
    configFile.close();
    xSemaphoreGive(_fileMutex); // Release the mutex before returning.
    return false;
  }

  // Close the file and confirm the save was successful.
  configFile.close();
  SerialLog::getInstance().print("Configuration saved.");
  xSemaphoreGive(_fileMutex); // Release the mutex.
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

  // After erasing, the NVS needs to be re-initialized.
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
  _isDirty = true;
}
