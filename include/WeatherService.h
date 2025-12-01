#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct WeatherData
{
  float temp;
  float feelsLike;
  float humidity;
  float windSpeed;
  float pressure;
  String condition;
  String iconUrl;
  bool isValid;
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
  void updateLocation();
  void updateWeather();

  WeatherData getCurrentWeather() const;

private:
  WeatherService();

  unsigned long _lastUpdate;
  unsigned long _lastLocationUpdate;
  WeatherData _currentWeather = {0.0, 0.0, 0.0, 0.0, 0.0, "", "", false};
  mutable SemaphoreHandle_t _mutex;
};
