// ConfigManager.cpp

#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Define the path for the configuration file on the LittleFS filesystem.
#define CONFIG_FILE "/config.json"

void ConfigManager::begin()
{
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
  wifiSSID = "";
  wifiPassword = "";
  hostname = "";
  autoBrightness = true;
  brightness = 128;        // Default brightness at 50%
  use24HourFormat = false; // Default to 12-hour format
  useCelsius = false;      // Default to Fahrenheit

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
  wifiSSID = doc["wifiSSID"] | "";
  wifiPassword = doc["wifiPassword"] | "";
  hostname = doc["hostname"] | "";
  autoBrightness = doc["autoBrightness"] | true;
  brightness = doc["brightness"] | 128;
  use24HourFormat = doc["use24HourFormat"] | false;
  useCelsius = doc["useCelsius"] | false;

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
      i++;
    }
  }

  SerialLog::getInstance().print("Configuration loaded successfully.");
}

bool ConfigManager::save()
{
  // Open the configuration file in write mode, which creates/truncates it.
  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile)
  {
    SerialLog::getInstance().print("Failed to open config file for writing.");
    return false;
  }

  // Create a JSON document to store the current settings.
  JsonDocument doc;
  doc["wifiSSID"] = wifiSSID;
  doc["wifiPassword"] = wifiPassword;
  doc["hostname"] = hostname;
  doc["autoBrightness"] = autoBrightness;
  doc["brightness"] = brightness;
  doc["use24HourFormat"] = use24HourFormat;
  doc["useCelsius"] = useCelsius;

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
  }

  // Serialize the JSON document into the file.
  if (serializeJson(doc, configFile) == 0)
  {
    SerialLog::getInstance().print("Failed to write to config file.");
    // Close the file and report the failure.
    configFile.close();
    return false;
  }

  // Close the file and confirm the save was successful.
  configFile.close();
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