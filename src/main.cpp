// main.cpp

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WebServer.h"
#include <WiFi.h>

// Include custom files
#include "ConfigManager.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "ntp.h"

// Constants
const unsigned long SENSOR_UPDATE_INTERVAL = 10000; // 10 seconds
const char *AP_SSID = "ESP32-Clock-Setup";

// Local variables
unsigned long prevSensorMillis = 0;
WebServer webServer;
bool isWifiConnected = false;

void setup()
{
  Serial.begin(115200);
  ConfigManager::getInstance().begin();
  setupSensors();

  auto &display = Display::getInstance();
  display.begin();
  display.drawStatusMessage("Initializing...");

  // --- WiFi Connection ---
  String ssid = ConfigManager::getInstance().getWifiSSID();
  String password = ConfigManager::getInstance().getWifiPassword();

  if (ssid.length() > 0)
  {
    display.drawStatusMessage("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000)
    { // 15-second timeout
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      isWifiConnected = true;
      Serial.println("\nWiFi connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      display.drawStatusMessage(("IP: " + WiFi.localIP().toString()).c_str());
      delay(2000);
    }
  }

  if (!isWifiConnected)
  {
    Serial.println("\nStarting AP mode.");
    WiFi.softAP(AP_SSID);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(apIP);
    display.drawStatusMessage(("Setup AP: " + String(AP_SSID)).c_str());
  }

  // Start the web server regardless of connection mode
  webServer.begin();

  if (isWifiConnected)
  {
    display.drawStatusMessage("Syncing Time...");
    TimeManager::getInstance().begin();
    display.drawLayout();
  }
  else
  {
    // If not connected, show a message and don't draw the full clock
    display.drawStatusMessage("Connect to AP & set WiFi");
  }
}

void loop()
{
  delay(10);
  unsigned long now = millis();
  auto &timeManager = TimeManager::getInstance();
  auto &display = Display::getInstance();

  if (isWifiConnected)
  {
    // Update time management
    timeManager.update();

    // Update brightness
    display.updateBrightness();

    // Update display with current time
    display.drawClock(timeManager.getFormattedTime().c_str(), timeManager.getTOD().c_str());
    display.drawDate(timeManager.getFormattedDate().c_str());
    display.drawDayOfWeek(timeManager.getDayOfWeek().c_str());

    // --- Sensor Update Logic ---
    if (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL)
    {
      prevSensorMillis = now;
      bool useCelsius = ConfigManager::getInstance().isCelsius();
      float temp = readTemperature(useCelsius);
      float hum = readHumidity();
      display.drawTemperature(temp, useCelsius);
      display.drawHumidity(hum);
    }
  }
}