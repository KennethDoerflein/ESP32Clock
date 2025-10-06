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
    Serial.println("An error occurred while mounting LittleFS. Halting.");
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
  autoBrightness = true;
  brightness = 128;        // Default brightness at 50%
  use24HourFormat = false; // Default to 12-hour format
  useCelsius = false;      // Default to Fahrenheit
  Serial.println("Loaded default configuration.");
}

void ConfigManager::load()
{
  // Check if the configuration file exists.
  if (!LittleFS.exists(CONFIG_FILE))
  {
    Serial.println("Config file not found, creating with defaults.");
    // If it doesn't exist, apply default settings and save them to a new file.
    setDefaults();
    save();
    return;
  }

  // Open the configuration file for reading.
  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile)
  {
    Serial.println("Failed to open config file for reading. Using defaults.");
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
    Serial.println(error.c_str());
    // If parsing fails, the file is likely corrupt. Use default settings.
    setDefaults();
    return;
  }

  // Load each setting from the JSON document.
  // The `|` operator provides a default value if the key is missing.
  wifiSSID = doc["wifiSSID"] | "";
  wifiPassword = doc["wifiPassword"] | "";
  autoBrightness = doc["autoBrightness"] | true;
  brightness = doc["brightness"] | 128;
  use24HourFormat = doc["use24HourFormat"] | false;
  useCelsius = doc["useCelsius"] | false;

  Serial.println("Configuration loaded successfully.");
}

bool ConfigManager::save()
{
  // Open the configuration file in write mode, which creates/truncates it.
  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing.");
    return false;
  }

  // Create a JSON document to store the current settings.
  JsonDocument doc;
  doc["wifiSSID"] = wifiSSID;
  doc["wifiPassword"] = wifiPassword;
  doc["autoBrightness"] = autoBrightness;
  doc["brightness"] = brightness;
  doc["use24HourFormat"] = use24HourFormat;
  doc["useCelsius"] = useCelsius;

  // Serialize the JSON document into the file.
  if (serializeJson(doc, configFile) == 0)
  {
    Serial.println("Failed to write to config file.");
    // Close the file and report the failure.
    configFile.close();
    return false;
  }

  // Close the file and confirm the save was successful.
  configFile.close();
  Serial.println("Configuration saved.");
  return true;
}