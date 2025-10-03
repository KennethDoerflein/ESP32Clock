// sensors.cpp

#include "sensors.h" // Include the header file
#include "config.h"

// Define the sensor objects
Adafruit_BME280 BME; // Global BME280 instance

RTC_Type RTC; // Defined using alias from sensors.h

void setupSensors()
{
  // Initialize sensors here
#ifndef WOKWI
  if (!BME.begin(0x76))
  { // I2C address can be 0x77 or 0x76
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
#endif

  if (!RTC.begin())
  {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }
}

void printBME280Data(bool useCelsius)
{
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
// For simulation, return a reasonable temperature
#ifdef WOKWI
  temperature = 22.5 + (random(-20, 21) / 10.0);
#else
  temperature = BME.readTemperature();
#endif

  if (!useCelsius)
  {
    temperature = (temperature * 9.0 / 5.0) + 32.0;
  }
  return temperature;
}

float readHumidity()
{
// For simulation, return a reasonable humidity
#ifdef WOKWI
  return 45.0 + (random(-50, 51) / 10.0);
#else
  return BME.readHumidity();
#endif
}