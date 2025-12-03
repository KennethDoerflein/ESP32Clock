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

  // Called by the background task
  void updateWeather();
  void notifyTaskFinished();
  void updateLocation();

  /**
   * @brief Converts wind direction in degrees to a cardinal direction string.
   * @param degrees Wind direction in degrees (0-360).
   * @return String representing the cardinal direction (e.g., "N", "NE").
   */
  static String getWindDirectionStr(int degrees);

  /**
   * @brief Resolves an address string to coordinates and a formatted name.
   *
   * @param query The address/location to search for.
   * @param resolvedAddress Output: The formatted address found (e.g., "Paris, France").
   * @param lat Output: Latitude.
   * @param lon Output: Longitude.
   * @return true if location found, false otherwise.
   */
  bool resolveLocation(const String &query, String &resolvedAddress, float &lat, float &lon);

private:
  WeatherService();

  WeatherData _currentWeather;
  unsigned long _lastUpdate;
  unsigned long _lastLocationUpdate;

  mutable SemaphoreHandle_t _mutex;
  TaskHandle_t _weatherTaskHandle;
};
