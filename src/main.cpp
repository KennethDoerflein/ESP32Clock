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
#include <WiFi.h>
#include <memory> // For std::unique_ptr

// Include all custom module headers.
#include "ConfigManager.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "ntp.h"
#include "WiFiManager.h"
#include "DisplayManager.h"
#include "pages/ClockPage.h"
#include "pages/InfoPage.h"
#include "ClockWebServer.h"

/**
 * @brief The main setup function, run once on boot.
 */
void setup()
{
  Serial.begin(115200);

  // Initialize managers and hardware.
  ConfigManager::getInstance().begin();
  auto &display = Display::getInstance();
  display.begin();

  setupSensors();
  display.drawStatusMessage("Initializing...");

  // Initialize WiFi. This will connect or start an AP.
  // The begin method returns true if it started the captive portal.
  bool captivePortalStarted = WiFiManager::getInstance().begin();

  // If captive portal is active, enable it on the web server.
  if (captivePortalStarted)
  {
    ClockWebServer::getInstance().enableCaptivePortal();
  }

  // Start the web server. It will now have the correct routes.
  ClockWebServer::getInstance().begin();

  // Initialize the Display Manager
  auto &displayManager = DisplayManager::getInstance();
  displayManager.begin(display.getTft());

  // Add pages to the manager. Ownership is moved to the manager.
  displayManager.addPage(std::make_unique<ClockPage>(&display.getTft()));
  displayManager.addPage(std::make_unique<InfoPage>());

  // If connected, set up the main display and sync time.
  if (WiFiManager::getInstance().isConnected())
  {
    display.drawStatusMessage("Syncing Time...");
    TimeManager::getInstance().begin();
    // Set the initial page
    displayManager.setPage(0);
  }
  else
  {
    display.drawStatusMessage("Connect to AP & set WiFi");
  }
}

/**
 * @brief The main application loop.
 */
void loop()
{
  static unsigned long lastPageChange = 0;
  static int currentPageIndex = 0;

  delay(100); // Small delay to yield to other tasks.

  // Handle DNS requests for the captive portal if it's active.
  WiFiManager::getInstance().handleDns();

  // Only run the main clock logic if WiFi is connected.
  if (WiFiManager::getInstance().isConnected())
  {
    auto &timeManager = TimeManager::getInstance();
    auto &display = Display::getInstance();
    auto &displayManager = DisplayManager::getInstance();

    // Perform periodic tasks.
    timeManager.update();
    display.updateBrightness();
    handleSensorUpdates();

    // Update the current display page
    displayManager.update();

    // Check if settings have changed and need a reload.
    auto &config = ConfigManager::getInstance();
    if (config.isDirty())
    {
      // Forcing a redraw of the current page is enough to apply
      // all visual settings (time format, temp unit, etc.)
      displayManager.setPage(displayManager.getCurrentPageIndex(), true);
      config.clearDirtyFlag();
      Serial.println("Configuration reloaded and page refreshed.");
    }

    // Simple page switching logic for until a button is added
    if (millis() - lastPageChange > 10000) // Switch every 10 seconds
    {
      lastPageChange = millis();
      currentPageIndex = (currentPageIndex + 1) % displayManager.getPagesSize();
      displayManager.setPage(currentPageIndex);
    }
  }
}