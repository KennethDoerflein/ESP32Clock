// config.h

#pragma once // Prevents the file from being included multiple times

// Temperature Unit
#define USE_CELSIUS false // Set to true for Celsius, false for Fahrenheit

// I2C Bus for Sensors (DS3231 & BME280)
#define I2C_SDA 8
#define I2C_SCL 9

// Active Buzzer
#define BUZZER_PIN 4

// Stop/Snooze Button
#define BUTTON_PIN 5