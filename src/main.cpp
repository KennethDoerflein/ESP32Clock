#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "DSEG7ModernBold64.h"

// Include custom files
#include "config.h"
#include "sensors.h"
#include "utils.h"

TFT_eSPI tft = TFT_eSPI();
unsigned long delayTime = 5000; // Default delay time of 5 seconds

void setup()
{
  Serial.begin(115200);
  delay(1000); // Wait for Serial to connect
  Serial.println("TFT_eSPI Font Test");

  printChipInfo();
  // setupSensors(); // Initialize sensors from sensors.cpp

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  // Set the reference point for drawing text to the Middle-Left of the string
  tft.setTextDatum(ML_DATUM);

  // Use our SCREEN_HEIGHT macro to calculate the vertical center
  int vertical_center = SCREEN_HEIGHT / 2;

  // Load a smooth font
  tft.loadFont(DSEG7ModernBold64);
  // --- Draw Hours and Minutes (Large Font) ---
  tft.drawString("12:34:59", 0, vertical_center);

  tft.unloadFont();
}

void loop()
{
  // printBME280Data(); // Print data using the function from sensors.cpp
  delay(delayTime);
}