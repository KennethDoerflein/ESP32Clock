// sensors.cpp

#include "sensors.h" // Include the header file

// Instantiate the global sensor objects declared in the header.
Adafruit_BME280 BME; // Global BME280 instance for temperature/humidity.
RTC_Type RTC;        // Global RTC instance, with type determined at compile time.

void setupSensors()
{
  // Initialize the BME280 sensor.
#ifndef WOKWI // The BME280 is not part of the Wokwi simulation.
  // Attempt to begin communication with the BME280 at I2C address 0x76.
  if (!BME.begin(0x76))
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    // Halt execution if the sensor is not found on physical hardware.
    while (1)
      ;
  }
#endif

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
  // Use conditional compilation to provide fake data for simulation
  // or read from the actual sensor on hardware.
#ifdef WOKWI
  // Generate a plausible, slightly fluctuating temperature for the simulator.
  temperature = 22.5 + (random(-20, 21) / 10.0);
#else
  // Read the real temperature from the BME280 sensor.
  temperature = BME.readTemperature();
#endif

  // Convert to Fahrenheit if requested.
  if (!useCelsius)
  {
    temperature = (temperature * 9.0 / 5.0) + 32.0;
  }
  return temperature;
}

float readHumidity()
{
  // Use conditional compilation for simulation vs. hardware.
#ifdef WOKWI
  // Generate a plausible, slightly fluctuating humidity for the simulator.
  return 45.0 + (random(-50, 51) / 10.0);
#else
  // Read the real humidity from the BME280 sensor.
  return BME.readHumidity();
#endif
}