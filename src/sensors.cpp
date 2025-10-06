// sensors.cpp

#include "sensors.h" // Include the header file
#include "ConfigManager.h"
#include "display.h"

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME; // Global BME280 instance for temperature/humidity.
RTC_Type RTC;        // Global RTC instance, with type determined at compile time.

/// @brief Stores the timestamp of the last sensor update for interval timing.
static unsigned long prevSensorMillis = 0;

void setupSensors()
{
  // Initialize the BME280 sensor.
  // Attempt to begin communication with the BME280 at I2C address 0x76.
  if (!BME.begin(0x76))
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    // Halt execution if the sensor is not found on physical hardware.
    while (1)
      ;
  }

  // Initialize the Real-Time Clock.
  if (!RTC.begin())
  {
    Serial.println("Couldn't find RTC");
    // Halt execution if the RTC is not found, as it's critical for timekeeping.
    while (1)
      ;
  }
}

void printBME280Data(bool useCelsius)
{
  // A utility function for debugging sensor readings via the Serial monitor.
  float temperature = readTemperature(useCelsius);
  float humidity = readHumidity();
  char unit = useCelsius ? 'C' : 'F';

  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.print(" °");
  Serial.println(unit);

  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.println();
}

float readTemperature(bool useCelsius)
{
  float temperature;
  // Read the real temperature from the BME280 sensor.
  temperature = BME.readTemperature();

  // Convert to Fahrenheit if requested.
  if (!useCelsius)
  {
    temperature = (temperature * 9.0 / 5.0) + 32.0;
  }
  return temperature;
}

float readHumidity()
{
  // Read the real humidity from the BME280 sensor.
  return BME.readHumidity();
}

void handleSensorUpdates()
{
  unsigned long now = millis();
  // Check if the defined interval has passed since the last update.
  if (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL)
  {
    prevSensorMillis = now; // Reset the timer.

    // Get the current temperature unit setting.
    bool useCelsius = ConfigManager::getInstance().isCelsius();

    // Read sensor data.
    float temp = readTemperature(useCelsius);
    float hum = readHumidity();

    // Update the display with the new sensor readings.
    auto &display = Display::getInstance();
    display.drawTemperature(temp, useCelsius);
    display.drawHumidity(hum);
  }
}