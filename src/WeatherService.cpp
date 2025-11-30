#include "WeatherService.h"
#include "ConfigManager.h"
#include "SerialLog.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static const unsigned long WEATHER_UPDATE_INTERVAL = 10 * 60 * 1000; // 10 minutes

// Task handle for the background update task
TaskHandle_t weatherTaskHandle = NULL;

// These store the lookup URL and the specific Station ID (e.g., "KJFK")
static String g_stationsUrl = "";
static String g_stationId = "";

void weatherUpdateTask(void *parameter)
{
  WeatherService *service = static_cast<WeatherService *>(parameter);
  service->updateWeather();
  vTaskDelete(NULL); // Delete self when done
  weatherTaskHandle = NULL;
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

        // Invalidate grid info so we refetch it next weather update
        _gridId = "";
        // Also invalidate our station ID since we moved
        g_stationsUrl = "";
        g_stationId = "";

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

  // If we don't have grid info, fetch it first
  if (_gridId.length() == 0)
  {
    if (!fetchGridData(lat, lon))
      return;
  }

  fetchForecastData();
}

bool WeatherService::fetchGridData(float lat, float lon)
{
  // DNS Check & Fallback
  IPAddress ip;
  const char *host = "api.weather.gov";
  if (WiFi.hostByName(host, ip))
  {
    SerialLog::getInstance().printf("Resolved %s to %s\n", host, ip.toString().c_str());
  }
  else
  {
    SerialLog::getInstance().printf("DNS resolution failed for %s. Current DNS: %s\n", host, WiFi.dnsIP().toString().c_str());
    SerialLog::getInstance().print("Attempting to switch to Google DNS (8.8.8.8)...\n");

    // Force DNS to 8.8.8.8 and 1.1.1.1 while keeping current IP config
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
    delay(1000);

    if (WiFi.hostByName(host, ip))
    {
      SerialLog::getInstance().printf("Resolved %s after DNS switch to %s\n", host, ip.toString().c_str());
    }
    else
    {
      SerialLog::getInstance().print("DNS resolution failed even after DNS switch.\n");
    }
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(30);

  String url = "https://api.weather.gov/points/" + String(lat, 4) + "," + String(lon, 4);

  int retries = 3;
  bool success = false; // Variable declared here for the loop

  while (retries > 0 && !success)
  {
    SerialLog::getInstance().printf("Fetching grid data (Attempt %d) from: %s (Heap: %d)\n", 4 - retries, url.c_str(), ESP.getFreeHeap());

    http.begin(client, url);
    http.setTimeout(15000);
    http.setReuse(false);
    http.setUserAgent("ESP32Clock/2.0 (contact@email.com)");
    http.addHeader("Accept", "application/geo+json");
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    int httpCode = http.GET();

    if (httpCode == 200)
    {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error)
      {
        _gridId = doc["properties"]["gridId"].as<String>();
        _gridX = doc["properties"]["gridX"];
        _gridY = doc["properties"]["gridY"];

        // Capture the observation stations URL so we can find the real thermometer later
        g_stationsUrl = doc["properties"]["observationStations"].as<String>();

        success = true; // Set success to exit loop
        SerialLog::getInstance().printf("Grid info: %s (%d, %d)\n", _gridId.c_str(), _gridX, _gridY);
      }
      else
      {
        SerialLog::getInstance().print("Failed to parse Grid JSON\n");
      }
    }
    else
    {
      char errBuf[64];
      client.lastError(errBuf, sizeof(errBuf));
      SerialLog::getInstance().printf("Grid data fetch failed. HTTP Code: %d (%s), SSL Error: %s\n", httpCode, http.errorToString(httpCode).c_str(), errBuf);
    }
    http.end();

    if (!success)
    {
      retries--;
      if (retries > 0)
        delay(2000); // Wait before retry
    }
  }
  return success;
}

bool WeatherService::fetchForecastData()
{
  // 1. If we don't have a station ID yet, we must find one first
  if (g_stationId.length() == 0)
  {
    if (g_stationsUrl.length() == 0)
    {
      SerialLog::getInstance().print("Missing Stations URL. Cannot fetch obs.\n");
      return false;
    }

    SerialLog::getInstance().print("Fetching Weather Station ID...\n");

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, g_stationsUrl);
    http.setUserAgent("ESP32Clock/2.0 (contact@email.com)");
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    int code = http.GET();
    if (code == 200)
    {
      WiFiClient *stream = http.getStreamPtr();

      // MANUALLY find the first station ID (JSON parsing was failing here)
      // We look for the pattern: "stationIdentifier": "XXXX"
      if (stream->find("\"stationIdentifier\""))
      {
        // Found the key, now look for the colon and opening quote
        if (stream->find(":"))
        {
          if (stream->find("\""))
          {
            // Read until the closing quote
            g_stationId = stream->readStringUntil('\"');
            SerialLog::getInstance().printf("Found Station ID: %s\n", g_stationId.c_str());
          }
        }
      }
    }
    else
    {
      SerialLog::getInstance().printf("Station fetch failed: %d\n", code);
    }
    http.end();

    // Safety check: if parsing failed or ID is invalid
    if (g_stationId.length() == 0 || g_stationId == "null")
    {
      SerialLog::getInstance().print("Failed to find a valid Station ID.\n");
      // Clear it so we try again next time
      g_stationId = "";
      return false;
    }
  }

  // 2. Fetch the Actual Observation (Current Temp) using the Station ID
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  // Construct URL for latest observation
  String url = "https://api.weather.gov/stations/" + g_stationId + "/observations/latest";
  SerialLog::getInstance().printf("Fetching Obs: %s\n", url.c_str());

  http.begin(client, url);
  http.setUserAgent("ESP32Clock/2.0 (contact@email.com)");
  http.addHeader("Accept", "application/geo+json");
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  bool success = false;

  if (httpCode == 200)
  {
    WiFiClient *stream = http.getStreamPtr();

    // Filter for just the data we need (Temperature is in Celsius)
    JsonDocument filter;
    filter["properties"]["temperature"]["value"] = true;
    filter["properties"]["textDescription"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, *stream, DeserializationOption::Filter(filter));

    if (!error)
    {
      JsonObject props = doc["properties"];
      // Check if temperature value exists (it can be null)
      if (!props["temperature"]["value"].isNull())
      {
        float tempC = props["temperature"]["value"].as<float>();
        const char *cond = props["textDescription"];

        // Convert Celsius to Fahrenheit
        _currentWeather.temp = (tempC * 9.0 / 5.0) + 32.0;
        _currentWeather.condition = (cond ? String(cond) : "Unknown");
        _currentWeather.isValid = true;
        success = true;

        SerialLog::getInstance().printf("OBSERVED Temp: %.1fF (%.1fC), %s\n",
                                        _currentWeather.temp, tempC, _currentWeather.condition.c_str());
      }
      else
      {
        SerialLog::getInstance().print("Temp value is null in observation\n");
      }
    }
    else
    {
      SerialLog::getInstance().printf("JSON Error: %s\n", error.c_str());
    }
  }
  else
  {
    SerialLog::getInstance().printf("Obs HTTP Failed: %d\n", httpCode);

    // If we get a 404, our station ID might be bad. Clear it to force a re-scan.
    if (httpCode == 404)
    {
      SerialLog::getInstance().print("Invalid Station ID, clearing cache.\n");
      g_stationId = "";
    }
  }

  http.end();
  return success;
}