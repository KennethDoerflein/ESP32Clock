// sensors.cpp

#include "sensors.h"
#include "ConfigManager.h"

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME;
RTC_Type RTC;

// Static variables to cache the latest sensor readings.
static float cached_temp_c = 0.0;
static float cached_humidity = 0.0;
static bool bme280_found = false; // Track BME280 sensor status

/// @brief Stores the timestamp of the last sensor update for interval timing.
static unsigned long prevSensorMillis = 0;

void setupSensors()
{
  bme280_found = BME.begin(0x76);
  if (!bme280_found)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    // The device will now rely on the RTC for temperature.
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
    if (bme280_found)
    {
      cached_temp_c = BME.readTemperature();
      cached_humidity = BME.readHumidity();
    }
    else
    {
      // Fallback to RTC temperature if BME280 is not available
      cached_temp_c = RTC.getTemperature();
      cached_humidity = -1; // Indicate that humidity is not available
    }
  }
}