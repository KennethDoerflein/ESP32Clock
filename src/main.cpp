// main.cpp

/**
 * @file main.cpp
 * @brief The main entry point for the ESP32 Clock application.
 *
 * This file contains the primary setup and loop functions that orchestrate
 * the device's operation, including initialization of all modules, handling
 * WiFi connectivity, and managing the main application logic loop.
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "ClockWebServer.h"
#include <WiFi.h>

// Include all custom module headers.
#include "ConfigManager.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "ntp.h"
#include "WiFiManager.h"

/**
 * @brief The main setup function, run once on boot.
 *
 * Initializes serial communication, managers, sensors, display, and WiFi.
 * It orchestrates the startup sequence, relying on dedicated managers for
 * specific tasks. It ensures that the web server is started and that the
 * time is synchronized if a WiFi connection is established.
 */
void setup()
{
  Serial.begin(115200);

  // Initialize managers and hardware.
  ConfigManager::getInstance().begin();
  setupSensors();
  auto &display = Display::getInstance();
  display.begin();
  display.drawStatusMessage("Initializing...");

  // Initialize WiFi. This will connect or start an AP.
  WiFiManager::getInstance().begin();

  // Start the web server.
  ClockWebServer::getInstance().begin();

  // If connected, set up the main display and sync time.
  if (WiFiManager::getInstance().isConnected())
  {
    display.drawStatusMessage("Syncing Time...");
    TimeManager::getInstance().begin();
    display.drawLayout();
  }
  else
  {
    // A message is already shown by WiFiManager, but we can affirm it.
    display.drawStatusMessage("Connect to AP & set WiFi");
  }
}

/**
 * @brief The main application loop.
 *
 * This function runs continuously. If WiFi is connected, it updates the
 * time, brightness, and display elements. It also periodically reads and
 * displays sensor data by calling the appropriate handlers. If WiFi is not
 * connected, it does nothing, yielding to system tasks.
 */
void loop()
{
  delay(10); // Small delay to yield to other tasks.

  // Handle DNS requests for the captive portal if it's active.
  WiFiManager::getInstance().handleDns();

  // Only run the main clock logic if WiFi is connected.
  if (WiFiManager::getInstance().isConnected())
  {
    auto &timeManager = TimeManager::getInstance();
    auto &display = Display::getInstance();

    // Perform periodic tasks.
    timeManager.update();
    display.updateBrightness();
    handleSensorUpdates(); // Sensor updates include redrawing.

    // Redraw the main display components.
    display.drawClock(timeManager.getFormattedTime().c_str(), timeManager.getTOD().c_str());
    display.drawDate(timeManager.getFormattedDate().c_str());
    display.drawDayOfWeek(timeManager.getDayOfWeek().c_str());
  }
}