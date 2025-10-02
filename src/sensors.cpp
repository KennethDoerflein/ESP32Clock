// sensors.cpp

#include "sensors.h" // Include the header file

// Define the sensor objects
Adafruit_BME280 bme;
RTC_DS3231 RTC;

void setupSensors()
{
  // Initialize sensors here
  if (!bme.begin(0x76))
  { // I2C address can be 0x77 or 0x76
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
}

void printBME280Data()
{
  // Read the actual sensor values
  float temperature = bme.readTemperature();
  float humidity = bme.readHumidity();

  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.println();
}