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
  float pressure = 0.0;
  String condition = "";
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

private:
  WeatherService();

  WeatherData _currentWeather;
  unsigned long _lastUpdate;
  unsigned long _lastLocationUpdate;

  mutable SemaphoreHandle_t _mutex;
  TaskHandle_t _weatherTaskHandle;
};
