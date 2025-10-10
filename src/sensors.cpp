// sensors.cpp

#include "sensors.h"
#include "ConfigManager.h"

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME;
RTC_Type RTC;

// Static variables to cache the latest sensor readings.
static float cached_temp_c = 0.0;
static float cached_humidity = 0.0;

/// @brief Stores the timestamp of the last sensor update for interval timing.
static unsigned long prevSensorMillis = 0;

void setupSensors()
{
  if (!BME.begin(0x76))
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }

  if (!RTC.begin())
  {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }
  // Perform an initial sensor read to populate cached values.
  handleSensorUpdates(true);
}

float readTemperature(bool useCelsius)
{
  float temperature = BME.readTemperature();
  if (!useCelsius)
  {
    temperature = (temperature * 9.0 / 5.0) + 32.0;
  }
  return temperature;
}

float readHumidity()
{
  return BME.readHumidity();
}

// Cached-value getter functions
float getTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_temp_c;
  }
  else
  {
    return (cached_temp_c * 9.0 / 5.0) + 32.0;
  }
}

float getHumidity()
{
  return cached_humidity;
}

void handleSensorUpdates(bool force)
{
  unsigned long now = millis();
  if (force || (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL))
  {
    prevSensorMillis = now;
    cached_temp_c = BME.readTemperature();
    cached_humidity = BME.readHumidity();
  }
}