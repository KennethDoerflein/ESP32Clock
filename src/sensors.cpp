// sensors.cpp

#include "sensors.h" // Include the header file

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

void printBME280Data()
{
  float temperature = readTemperature();
  float humidity = readHumidity();

  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.println();
}

float readTemperature()
{
// For simulation, return a reasonable temperature
#ifdef WOKWI
  return 22.5 + (random(-20, 21) / 10.0);
#else
  return BME.readTemperature();
#endif
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