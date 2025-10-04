#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Include custom files
#include "config.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "ntp.h"

// Constants
const unsigned long SENSOR_UPDATE_INTERVAL = 10000; // 10 seconds

// Local variables
unsigned long prevSensorMillis = 0;

void setup()
{
  Serial.begin(115200);
  setupSensors();

  auto &display = Display::getInstance();
  display.begin();

  display.drawStatusMessage("Connecting to WiFi...");

#ifndef WOKWI
  // Use WiFiManager to connect to WiFi. If it fails to connect,
  // it will start a configuration portal with the name "ESP32-Clock-Setup".
  WiFiManager wm;
  if (!wm.autoConnect("ESP32-Clock-Setup"))
  {
    Serial.println("Failed to connect and hit timeout");
    display.drawStatusMessage("Config Failed. Restarting...");
    delay(3000);
    ESP.restart();
  }
#else
  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
#endif
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());

  // Update the on-screen message before syncing
  display.drawStatusMessage("Syncing Time...");

  // Initialize time management
  TimeManager::getInstance().begin();

  // Draw the main layout
  display.drawLayout();
}

void loop()
{
  delay(10);
  unsigned long now = millis();
  auto &timeManager = TimeManager::getInstance();
  auto &display = Display::getInstance();

  // Update time management
  timeManager.update();

  // Update display with current time
  display.drawClock(timeManager.getFormattedTime().c_str(), timeManager.getTOD().c_str());
  display.drawDate(timeManager.getFormattedDate().c_str());
  display.drawDayOfWeek(timeManager.getDayOfWeek().c_str());

  // --- Sensor Update Logic ---
  // Runs every 10 seconds
  if (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL)
  {
    prevSensorMillis = now;
    float temp = readTemperature(USE_CELSIUS);
    float hum = readHumidity();
    display.drawTemperature(temp, USE_CELSIUS);
    display.drawHumidity(hum);
  }
}