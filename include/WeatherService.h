#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct WeatherData
{
  float temp = 0.0;
  float feelsLike = 0.0;
  float humidity = 0.0;
  float windSpeed = 0.0;
  int rainChance = 0;
  char condition[32] = "";

  float uvIndex = 0.0;
  int cloudCover = 0;
  float pressure = 0.0;
  float visibility = 0.0;
  int windDirection = 0;
  float windGusts = 0.0;
  char sunrise[16] = "";
  char sunset[16] = "";

  bool isValid = false;
};

class WeatherService
{
public:
  static WeatherService &getInstance()
  {
    static WeatherService instance;
    return instance;
  }

  void begin();
  void loop();
  WeatherData getCurrentWeather() const;

  // Called by the persistent background task
  void updateWeather();
  void updateLocation();

  /**
   * @brief Signals the background task to perform an immediate weather update.
   * Thread-safe and non-blocking.
   */
  void forceUpdate();

  /**
   * @brief Converts wind direction in degrees to a cardinal direction string.
   * @param degrees Wind direction in degrees (0-360).
   * @return String representing the cardinal direction (e.g., "N", "NE").
   */
  static inline String getWindDirectionStr(int degrees)
  {
    const char *directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    // Clamp to [0, 360) to prevent negative modulo giving a negative array index.
    degrees = ((degrees % 360) + 360) % 360;
    int index = (int)((degrees + 22.5) / 45.0) % 8;
    return directions[index];
  }

  /**
   * @brief Converts WMO Weather Code to string logic.
   * @param code The WMO weather code.
   * @return A const string describing the condition.
   */
  static inline const char *getConditionFromWMO(int code)
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

  static inline String urlEncode(String str)
  {
    String encodedString;
    encodedString.reserve(str.length() * 3);
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

  static inline bool checkWordPresence(const String &text, const String &word)
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

  /**
   * @brief Resolves an address string to coordinates and a formatted name.
   * This is a non-blocking request. The actual resolution happens in the background.
   *
   * @param query The address/location to search for.
   * @return true if the request was successfully queued, false otherwise.
   */
  bool resolveLocationAsync(const String &query);

  struct GeocodingResult {
      bool success = false;
      String resolvedAddress;
      float lat = 0.0;
      float lon = 0.0;
      bool pending = false;
  };

  GeocodingResult getGeocodingResult();

private:
  WeatherService();

  /// @brief Entry point for the persistent weather task. Runs forever.
  static void weatherTaskEntry(void *parameter);

  /// @brief Blocking geocoding helper. Called only from weatherTaskEntry.
  bool resolveLocation(const String &query, String &resolvedAddress, float &lat, float &lon);

  // Weather state
  WeatherData _currentWeather;
  unsigned long _lastUpdate = 0;
  uint8_t _failureCount = 0; ///< Number of consecutive failed updates

  mutable SemaphoreHandle_t _mutex;
  TaskHandle_t _weatherTaskHandle;
  SemaphoreHandle_t _wakeSignal; ///< Binary semaphore to wake the persistent task

  // Geocoding request state
  String _geocodingQuery;
  GeocodingResult _geocodingResult;
};

inline String urlEncode(String str) { return WeatherService::urlEncode(str); }
inline bool checkWordPresence(const String &text, const String &word) { return WeatherService::checkWordPresence(text, word); }

