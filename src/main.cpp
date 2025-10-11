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
#include "AlarmManager.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "ntp.h"
#include "WiFiManager.h"
#include "DisplayManager.h"
#include "pages/ClockPage.h"
#include "pages/InfoPage.h"
#include "ClockWebServer.h"

// --- Pin Definitions ---
#define SNOOZE_BUTTON_PIN 5

// --- Global Variables for Button Handling ---
volatile unsigned long buttonPressTime = 0;
volatile unsigned long buttonReleaseTime = 0;
volatile bool buttonState = HIGH; // Start with the button not pressed

// --- Interrupt Service Routine (ISR) ---
void IRAM_ATTR handleButtonInterrupt()
{
  buttonState = digitalRead(SNOOZE_BUTTON_PIN);
  if (buttonState == LOW)
  { // Button was just pressed
    buttonPressTime = millis();
  }
  else
  { // Button was just released
    buttonReleaseTime = millis();
  }
}

/**
 * @brief The main setup function, run once on boot.
 */
void setup()
{
  Serial.begin(115200);

  // Initialize managers and hardware.
  ConfigManager::getInstance().begin();

  // Initialize the snooze button
  pinMode(SNOOZE_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN), handleButtonInterrupt, CHANGE);

  // Initialize the Alarm Manager
  AlarmManager::getInstance().begin();

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
    auto &alarmManager = AlarmManager::getInstance();

    // Perform periodic tasks.
    alarmManager.update();
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

    // --- Button Handling Logic ---
    if (buttonReleaseTime > 0)
    {
      unsigned long pressDuration = buttonReleaseTime - buttonPressTime;

      if (alarmManager.isRinging())
      {
        if (pressDuration > 10000)
        { // Long press (> 10 seconds)
          alarmManager.dismiss();
        }
        else
        { // Short press
          alarmManager.snooze();
        }
      }
      else
      {
        // If not ringing, a short press cycles pages.
        int newIndex = (displayManager.getCurrentPageIndex() + 1) % displayManager.getPagesSize();
        displayManager.setPage(newIndex);
      }

      // Reset timers
      buttonPressTime = 0;
      buttonReleaseTime = 0;
    }

    // --- Update Alarm Icon ---
    bool anyAlarmEnabled = false;
    for (int i = 0; i < config.getNumAlarms(); ++i)
    {
      if (config.getAlarm(i).enabled)
      {
        anyAlarmEnabled = true;
        break;
      }
    }
    displayManager.drawAlarmIcon(anyAlarmEnabled);
  }
}