// sensors.cpp

#include "sensors.h"
#include "ConfigManager.h"
#include "driver/temp_sensor.h"

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME;
RTC_Type RTC;

// Static variables to cache the latest sensor readings.
static float cached_bme_temp_c = 0.0;
static float cached_rtc_temp_c = 0.0;
static float cached_core_temp_c = 0.0;
static float cached_humidity = 0.0;
static bool bme280_found = false; // Track BME280 sensor status
static bool rtc_found = false;    // Track RTC status

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

  rtc_found = RTC.begin();
  if (!rtc_found)
  {
    Serial.println("Couldn't find RTC");
    // The main loop will now handle the error message.
    return; // Exit early, no point in continuing
  }

  // Initialize the internal temperature sensor
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();

  // Perform an initial sensor read to populate cached values.
  handleSensorUpdates(true);
}

bool isRtcFound()
{
  return rtc_found;
}

bool isBmeFound()
{
  return bme280_found;
}

// Cached-value getter functions
float getTemperature()
{
  if (bme280_found)
  {
    return getBmeTemperature();
  }
  else
  {
    return getRtcTemperature();
  }
}

float getHumidity()
{
  return cached_humidity;
}

float getBmeTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_bme_temp_c;
  }
  else
  {
    return (cached_bme_temp_c * 9.0 / 5.0) + 32.0;
  }
}

float getRtcTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_rtc_temp_c;
  }
  else
  {
    return (cached_rtc_temp_c * 9.0 / 5.0) + 32.0;
  }
}

float getCoreTemperature()
{
  bool useCelsius = ConfigManager::getInstance().isCelsius();
  if (useCelsius)
  {
    return cached_core_temp_c;
  }
  else
  {
    return (cached_core_temp_c * 9.0 / 5.0) + 32.0;
  }
}

void handleSensorUpdates(bool force)
{
  unsigned long now = millis();
  if (force || (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL))
  {
    prevSensorMillis = now;
    if (bme280_found)
    {
      cached_bme_temp_c = BME.readTemperature();
      cached_humidity = BME.readHumidity();
    }
    else
    {
      cached_humidity = -1; // Indicate that humidity is not available
    }

    if (rtc_found)
    {
      cached_rtc_temp_c = RTC.getTemperature();
    }
    temp_sensor_read_celsius(&cached_core_temp_c);
  }
}