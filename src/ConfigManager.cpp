#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#define CONFIG_FILE "/config.json"

void ConfigManager::begin()
{
  if (!LittleFS.begin())
  {
    Serial.println("An error occurred while mounting LittleFS. Formatting...");
    if (LittleFS.format())
    {
      Serial.println("LittleFS formatted successfully.");
      LittleFS.begin(); // Retry mounting
    }
    else
    {
      Serial.println("Formatting failed. Halting.");
      while (1)
        ; // Halt
    }
  }
  load();
}

void ConfigManager::setDefaults()
{
  autoBrightness = true;
  brightness = 128;
  use24HourFormat = false;
  useCelsius = false;
  Serial.println("Loaded default configuration.");
}

void ConfigManager::load()
{
  if (!LittleFS.exists(CONFIG_FILE))
  {
    Serial.println("Config file not found, creating with defaults.");
    setDefaults();
    save();
    return;
  }

  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile)
  {
    Serial.println("Failed to open config file for reading. Using defaults.");
    setDefaults();
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error)
  {
    Serial.print("Failed to parse config file: ");
    Serial.println(error.c_str());
    setDefaults();
    return;
  }

  // Load settings, using defaults if a key is missing
  autoBrightness = doc["autoBrightness"] | true;
  brightness = doc["brightness"] | 128;
  use24HourFormat = doc["use24HourFormat"] | false;
  useCelsius = doc["useCelsius"] | false;

  Serial.println("Configuration loaded successfully.");
}

bool ConfigManager::save()
{
  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing.");
    return false;
  }

  JsonDocument doc;
  doc["autoBrightness"] = autoBrightness;
  doc["brightness"] = brightness;
  doc["use24HourFormat"] = use24HourFormat;
  doc["useCelsius"] = useCelsius;

  if (serializeJson(doc, configFile) == 0)
  {
    Serial.println("Failed to write to config file.");
    configFile.close();
    return false;
  }

  configFile.close();
  Serial.println("Configuration saved.");
  return true;
}