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
  String condition = "";

  float uvIndex = 0.0;
  int cloudCover = 0;
  float pressure = 0.0;
  float visibility = 0.0;
  int windDirection = 0;
  float windGusts = 0.0;
  String sunrise = "";
  String sunset = "";

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
  static String getWindDirectionStr(int degrees);

  /**
   * @brief Converts WMO Weather Code to string logic.
   * @param code The WMO weather code.
   * @return A const string describing the condition.
   */
  static const char *getConditionFromWMO(int code);

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

  WeatherData _currentWeather;
  unsigned long _lastUpdate;

  mutable SemaphoreHandle_t _mutex;
  TaskHandle_t _weatherTaskHandle;
  SemaphoreHandle_t _wakeSignal; ///< Binary semaphore to wake the persistent task

  // Geocoding request state
  String _geocodingQuery;
  GeocodingResult _geocodingResult;
};
