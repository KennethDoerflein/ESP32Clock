#include "WeatherService.h"
#include "ConfigManager.h"
#include "SerialLog.h"
#include "TimeManager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LockGuard.h"

static const unsigned long WEATHER_UPDATE_INTERVAL = 10 * 60 * 1000; // 10 minutes

void weatherUpdateTask(void *parameter)
{
  WeatherService *service = static_cast<WeatherService *>(parameter);
  service->updateWeather();
  service->notifyTaskFinished();
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

// Helper to URL encode a string
String urlEncode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
    {
      encodedString += c;
    }
    else if (c == ' ')
    {
      encodedString += '+';
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = (c) + '0';
      if (c > 9)
      {
        code0 = (c)-10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

WeatherService::WeatherService() : _lastUpdate(0), _lastLocationUpdate(0), _weatherTaskHandle(NULL)
{
  _mutex = xSemaphoreCreateMutex();
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

  bool isTaskRunning = false;
  {
    LockGuard lock(_mutex);
    isTaskRunning = (_weatherTaskHandle != NULL);
  }

  // Check if it's time to update and if a task isn't already running
  if ((now - _lastUpdate > WEATHER_UPDATE_INTERVAL || _lastUpdate == 0) && !isTaskRunning)
  {
    _lastUpdate = now;

    SerialLog::getInstance().print("Starting Weather Update Task...\n");

    // Create a FreeRTOS task pinned to Core 0
    // We protect the handle assignment
    LockGuard lock(_mutex);
    xTaskCreatePinnedToCore(
        weatherUpdateTask,   // Function to implement the task
        "WeatherUpdate",     // Name of the task
        12288,               // Stack size (12KB is sufficient for HTTPS)
        this,                // Task input parameter
        1,                   // Priority
        &_weatherTaskHandle, // Task handle
        0                    // Core 0
    );
  }
}

void WeatherService::notifyTaskFinished()
{
  LockGuard lock(_mutex);
  _weatherTaskHandle = NULL;
}

WeatherData WeatherService::getCurrentWeather() const
{
  LockGuard lock(_mutex);
  return _currentWeather;
}

struct StateMap
{
  const char *name;
  const char *code;
};

const StateMap US_STATES[] = {
    {"alabama", "al"}, {"alaska", "ak"}, {"arizona", "az"}, {"arkansas", "ar"}, {"california", "ca"}, {"colorado", "co"}, {"connecticut", "ct"}, {"delaware", "de"}, {"florida", "fl"}, {"georgia", "ga"}, {"hawaii", "hi"}, {"idaho", "id"}, {"illinois", "il"}, {"indiana", "in"}, {"iowa", "ia"}, {"kansas", "ks"}, {"kentucky", "ky"}, {"louisiana", "la"}, {"maine", "me"}, {"maryland", "md"}, {"massachusetts", "ma"}, {"michigan", "mi"}, {"minnesota", "mn"}, {"mississippi", "ms"}, {"missouri", "mo"}, {"montana", "mt"}, {"nebraska", "ne"}, {"nevada", "nv"}, {"new hampshire", "nh"}, {"new jersey", "nj"}, {"new mexico", "nm"}, {"new york", "ny"}, {"north carolina", "nc"}, {"north dakota", "nd"}, {"ohio", "oh"}, {"oklahoma", "ok"}, {"oregon", "or"}, {"pennsylvania", "pa"}, {"rhode island", "ri"}, {"south carolina", "sc"}, {"south dakota", "sd"}, {"tennessee", "tn"}, {"texas", "tx"}, {"utah", "ut"}, {"vermont", "vt"}, {"virginia", "va"}, {"washington", "wa"}, {"west virginia", "wv"}, {"wisconsin", "wi"}, {"wyoming", "wy"}, {"district of columbia", "dc"}};

// Check if a word exists in text as a whole word
bool checkWordPresence(const String &text, const String &word)
{
  int index = -1;
  while ((index = text.indexOf(word, index + 1)) != -1)
  {
    bool startOk = (index == 0) || !isAlphaNumeric(text.charAt(index - 1));
    bool endOk = (index + word.length() == text.length()) || !isAlphaNumeric(text.charAt(index + word.length()));
    if (startOk && endOk)
      return true;
  }
  return false;
}

// Helper function for the search logic
bool performGeocodingSearch(String url, String context, String &resolvedAddress, float &lat, float &lon)
{
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  SerialLog::getInstance().printf("Resolving Location: %s\n", url.c_str());

  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  bool success = false;

  if (httpCode == 200)
  {
    String payload = http.getString();
    if (payload.length() > 0)
    {
      // Create a filter to only deserialize what we need
      JsonDocument filter;
      filter["results"][0]["name"] = true;
      filter["results"][0]["latitude"] = true;
      filter["results"][0]["longitude"] = true;
      filter["results"][0]["country"] = true;
      filter["results"][0]["country_code"] = true;
      filter["results"][0]["admin1"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (!error)
      {
        JsonArray results = doc["results"];
        if (results.size() > 0)
        {
          int bestIndex = 0;

          // If we have a context string (e.g. "Paris, Texas"), try to find the best scoring match
          if (context.length() > 0)
          {
            String contextLower = context;
            contextLower.toLowerCase();
            int maxScore = -1;

            for (size_t i = 0; i < results.size(); i++)
            {
              String country = results[i]["country"].as<String>();
              String countryCode = results[i]["country_code"].as<String>();
              String admin1 = results[i]["admin1"].as<String>();
              country.toLowerCase();
              admin1.toLowerCase();

              int score = 0;

              // Direct matches
              if (admin1.length() > 0 && contextLower.indexOf(admin1) != -1)
              {
                score += 10;
              }
              if (country.length() > 0 && contextLower.indexOf(country) != -1)
              {
                score += 1;
              }

              // Abbreviation match for US
              if (countryCode == "US" && admin1.length() > 0)
              {
                for (const auto &state : US_STATES)
                {
                  if (admin1 == state.name)
                  {
                    if (checkWordPresence(contextLower, state.code))
                    {
                      score += 10;
                    }
                    break;
                  }
                }
              }

              if (score > maxScore)
              {
                maxScore = score;
                bestIndex = i;
              }
            }
            if (maxScore > 0)
            {
              SerialLog::getInstance().printf("Best context match at index %d (Score: %d)\n", bestIndex, maxScore);
            }
          }

          lat = results[bestIndex]["latitude"];
          lon = results[bestIndex]["longitude"];

          String name = results[bestIndex]["name"].as<String>();
          String country = results[bestIndex]["country"].as<String>();
          String admin1 = results[bestIndex]["admin1"].as<String>();

          resolvedAddress = name;
          if (admin1.length() > 0 && admin1 != name)
            resolvedAddress += ", " + admin1;
          if (country.length() > 0)
            resolvedAddress += ", " + country;

          success = true;
          SerialLog::getInstance().printf("Found: %s (%.4f, %.4f)\n", resolvedAddress.c_str(), lat, lon);
        }
        else
        {
          SerialLog::getInstance().print("No results in Geocoding response.\n");
        }
      }
      else
      {
        SerialLog::getInstance().printf("JSON Error: %s\n", error.c_str());
      }
    }
    else
    {
      SerialLog::getInstance().print("Empty payload received.\n");
    }
  }
  else
  {
    SerialLog::getInstance().printf("Geocoding HTTP Failed: %d\n", httpCode);
  }
  http.end();
  return success;
}

bool WeatherService::resolveLocation(const String &query, String &resolvedAddress, float &lat, float &lon)
{
  if (WiFi.status() != WL_CONNECTED || query.length() == 0)
    return false;

  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(query) + "&count=1&language=en&format=json";

  if (performGeocodingSearch(url, "", resolvedAddress, lat, lon))
  {
    return true;
  }

  int commaIndex = query.indexOf(',');
  if (commaIndex != -1)
  {
    String city = query.substring(0, commaIndex);
    url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncode(city) + "&count=10&language=en&format=json";

    if (performGeocodingSearch(url, query, resolvedAddress, lat, lon))
    {
      return true;
    }
  }

  return false;
}

void WeatherService::updateLocation()
{
  String address = ConfigManager::getInstance().getAddress();
  if (address.length() == 0)
    return;

  SerialLog::getInstance().printf("Updating location for: %s\n", address.c_str());

  String resolved;
  float lat, lon;

  if (resolveLocation(address, resolved, lat, lon))
  {
    ConfigManager::getInstance().setLat(lat);
    ConfigManager::getInstance().setLon(lon);
    SerialLog::getInstance().printf("Location resolved: %s (%.4f, %.4f)\n", resolved.c_str(), lat, lon);
    updateWeather();
  }
  else
  {
    SerialLog::getInstance().print("Failed to resolve location.\n");
  }
}

void WeatherService::updateWeather()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  float lat = ConfigManager::getInstance().getLat();
  float lon = ConfigManager::getInstance().getLon();

  if (lat == 0.0 && lon == 0.0)
  {
    // Try to get location if we have address
    if (ConfigManager::getInstance().getAddress().length() > 0)
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
               "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m," +
               "cloud_cover,pressure_msl,wind_direction_10m,wind_gusts_10m," +
               "uv_index,visibility,precipitation_probability" +
               "&daily=sunrise,sunset" +
               "&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch" +
               "&forecast_days=1&timezone=auto";

  SerialLog::getInstance().printf("Fetching Weather: %s\n", url.c_str());

  http.begin(client, url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // Good practice
  int httpCode = http.GET();

  if (httpCode == 200)
  {
    String payload = http.getString();
    JsonDocument doc;

    // Filter to reduce memory usage
    JsonDocument filter;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["apparent_temperature"] = true;
    filter["current"]["relative_humidity_2m"] = true;
    filter["current"]["wind_speed_10m"] = true;
    filter["current"]["weather_code"] = true;

    filter["current"]["cloud_cover"] = true;
    filter["current"]["pressure_msl"] = true;
    filter["current"]["wind_direction_10m"] = true;
    filter["current"]["wind_gusts_10m"] = true;

    filter["current"]["uv_index"] = true;
    filter["current"]["visibility"] = true;
    filter["current"]["precipitation_probability"] = true;

    filter["daily"]["sunrise"] = true;
    filter["daily"]["sunset"] = true;

    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (!error)
    {
      float temp = doc["current"]["temperature_2m"];
      float feelsLike = doc["current"]["apparent_temperature"];
      float humidity = doc["current"]["relative_humidity_2m"];
      float windSpeed = doc["current"]["wind_speed_10m"];
      int code = doc["current"]["weather_code"];

      int cloudCover = doc["current"]["cloud_cover"] | 0;
      float pressure = doc["current"]["pressure_msl"] | 0.0;
      int windDirection = doc["current"]["wind_direction_10m"] | 0;
      float windGusts = doc["current"]["wind_gusts_10m"] | 0.0;

      int rainChance = doc["current"]["precipitation_probability"] | 0;
      float uvIndex = doc["current"]["uv_index"] | 0.0;
      float visibility = doc["current"]["visibility"] | 0.0;

      String sunrise = "";
      String sunset = "";
      if (!doc["daily"]["sunrise"].isNull() && doc["daily"]["sunrise"].size() > 0)
      {
        String raw = doc["daily"]["sunrise"][0].as<String>();
        int tIndex = raw.indexOf('T');
        if (tIndex != -1)
          sunrise = raw.substring(tIndex + 1);
        else
          sunrise = raw;
      }
      if (!doc["daily"]["sunset"].isNull() && doc["daily"]["sunset"].size() > 0)
      {
        String raw = doc["daily"]["sunset"][0].as<String>();
        int tIndex = raw.indexOf('T');
        if (tIndex != -1)
          sunset = raw.substring(tIndex + 1);
        else
          sunset = raw;
      }

      {
        LockGuard lock(_mutex);
        _currentWeather.temp = temp;
        _currentWeather.feelsLike = feelsLike;
        _currentWeather.humidity = humidity;
        _currentWeather.windSpeed = windSpeed;
        _currentWeather.rainChance = rainChance;
        _currentWeather.condition = getConditionFromWMO(code);

        _currentWeather.uvIndex = uvIndex;
        _currentWeather.cloudCover = cloudCover;
        _currentWeather.pressure = pressure;
        _currentWeather.visibility = visibility;
        _currentWeather.windDirection = windDirection;
        _currentWeather.windGusts = windGusts;
        _currentWeather.sunrise = sunrise;
        _currentWeather.sunset = sunset;

        _currentWeather.isValid = true;
      }

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
