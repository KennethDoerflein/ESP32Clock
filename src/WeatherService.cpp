#include "WeatherService.h"
#include "ConfigManager.h"
#include "SerialLog.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const unsigned long WEATHER_UPDATE_INTERVAL = 5 * 60 * 1000; // 5 minutes

// Task handle for the background update task
TaskHandle_t weatherTaskHandle = NULL;

void weatherUpdateTask(void *parameter)
{
  WeatherService *service = static_cast<WeatherService *>(parameter);
  service->updateWeather();
  weatherTaskHandle = NULL;
  vTaskDelete(NULL); // Delete self when done
}

// Helper to convert WMO Weather Codes to String Condition
String getConditionFromWMO(int code)
{
  switch (code)
  {
  case 0:
    return "Clear";
  case 1:
    return "Mainly Clear";
  case 2:
    return "Partly Cloudy";
  case 3:
    return "Overcast";
  case 45:
  case 48:
    return "Fog";
  case 51:
  case 53:
  case 55:
    return "Drizzle";
  case 56:
  case 57:
    return "Freezing Drizzle";
  case 61:
  case 63:
  case 65:
    return "Rain";
  case 66:
  case 67:
    return "Freezing Rain";
  case 71:
  case 73:
  case 75:
    return "Snow";
  case 77:
    return "Snow Grains";
  case 80:
  case 81:
  case 82:
    return "Rain Showers";
  case 85:
  case 86:
    return "Snow Showers";
  case 95:
  case 96:
  case 99:
    return "Thunderstorm";
  default:
    return "Unknown";
  }
}

void WeatherService::begin()
{
  // Initial check handled in loop or triggered by events
}

void WeatherService::loop()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  unsigned long now = millis();
  // Check if it's time to update and if a task isn't already running
  if ((now - _lastUpdate > WEATHER_UPDATE_INTERVAL || _lastUpdate == 0) && weatherTaskHandle == NULL)
  {
    _lastUpdate = now;

    // Create a FreeRTOS task pinned to Core 0
    xTaskCreatePinnedToCore(
        weatherUpdateTask,  // Function to implement the task
        "WeatherUpdate",    // Name of the task
        40960,              // Stack size (40KB)
        this,               // Task input parameter
        1,                  // Priority
        &weatherTaskHandle, // Task handle
        0                   // Core 0
    );
  }
}

void WeatherService::updateLocation()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    SerialLog::getInstance().print("WiFi not connected. Cannot update location.\n");
    return;
  }

  String zip = ConfigManager::getInstance().getZipCode();
  if (zip.length() == 0)
    return;

  SerialLog::getInstance().printf("Updating location for Zip: %s\n", zip.c_str());

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  // using api.zippopotam.us
  String url = "https://api.zippopotam.us/us/" + zip;

  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      JsonArray places = doc["places"];
      if (places.size() > 0)
      {
        float lat = places[0]["latitude"].as<float>();
        float lon = places[0]["longitude"].as<float>();

        ConfigManager::getInstance().setLat(lat);
        ConfigManager::getInstance().setLon(lon);

        SerialLog::getInstance().printf("Location updated: %.4f, %.4f\n", lat, lon);

        updateWeather(); // Force weather update immediately
      }
    }
    else
    {
      SerialLog::getInstance().print("Failed to parse location JSON\n");
    }
  }
  else
  {
    SerialLog::getInstance().printf("Location update failed. HTTP Code: %d\n", httpCode);
  }
  http.end();
}

void WeatherService::updateWeather()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  float lat = ConfigManager::getInstance().getLat();
  float lon = ConfigManager::getInstance().getLon();

  if (lat == 0.0 && lon == 0.0)
  {
    // Try to get location if we have zip
    if (ConfigManager::getInstance().getZipCode().length() > 0)
    {
      updateLocation();
      return;
    }
    return; // No location data
  }

  // Fetch from Open-Meteo
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) +
               "&current=temperature_2m,weather_code&temperature_unit=fahrenheit";

  SerialLog::getInstance().printf("Fetching Weather: %s\n", url.c_str());

  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // Good practice
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error)
    {
      float temp = doc["current"]["temperature_2m"];
      int code = doc["current"]["weather_code"];

      _currentWeather.temp = temp;
      _currentWeather.condition = getConditionFromWMO(code);
      _currentWeather.isValid = true;

      SerialLog::getInstance().printf("Weather Updated: %.1fF, %s\n", temp, _currentWeather.condition.c_str());
    }
    else
    {
      SerialLog::getInstance().printf("JSON Error: %s\n", error.c_str());
    }
  }
  else
  {
    SerialLog::getInstance().printf("Weather HTTP Failed: %d\n", httpCode);
  }
  http.end();
}
