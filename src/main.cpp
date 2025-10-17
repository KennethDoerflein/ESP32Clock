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
#include <ESPmDNS.h>

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
#include "UpdateManager.h"

// --- Pin Definitions ---
#define SNOOZE_BUTTON_PIN 5
#define LOOP_INTERVAL 100 // Interval for the main loop in milliseconds

// --- Global Variables for Timers & Button Handling ---
unsigned long lastLoopTime = 0;
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

  // Add a small delay for web server to stabilize before mDNS announcement.
  delay(100);

  // Initialize the Display Manager
  auto &displayManager = DisplayManager::getInstance();
  displayManager.begin(display.getTft());

  // Add pages to the manager. Ownership is moved to the manager.
  displayManager.addPage(std::make_unique<ClockPage>(&display.getTft()));
  displayManager.addPage(std::make_unique<InfoPage>());

  // If connected, set up the main display and sync time.
  if (WiFiManager::getInstance().isConnected())
  {
    // --- mDNS Initialization ---
    // Start the mDNS responder for ESP32Clock_XXXXXX.local
    String hostname = WiFiManager::getInstance().getHostname();
    if (MDNS.begin(hostname.c_str()))
    {
      Serial.println("mDNS responder started");
      MDNS.addService("http", "tcp", 80); // Advertise the web server
    }
    else
    {
      Serial.println("Error starting mDNS!");
    }

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
  // Handle DNS requests for the captive portal if it's active.
  WiFiManager::getInstance().handleDns();

  // Implement a non-blocking delay.
  unsigned long currentMillis = millis();
  if (currentMillis - lastLoopTime < LOOP_INTERVAL)
  {
    // Yield to other tasks, especially the web server, when not busy.
    // A small delay helps prevent watchdog timeouts on ESP32.
    delay(1);
    return;
  }
  lastLoopTime = currentMillis;

  // Only run the main clock logic if WiFi is connected.
  if (WiFiManager::getInstance().isConnected())
  {
    auto &timeManager = TimeManager::getInstance();
    auto &display = Display::getInstance();
    auto &displayManager = DisplayManager::getInstance();
    auto &alarmManager = AlarmManager::getInstance();

    // Perform periodic tasks.
    alarmManager.update();
    bool timeUpdated = timeManager.update();
    timeManager.checkAlarms();
    timeManager.updateNtp();
    display.updateBrightness();
    handleSensorUpdates();

    // Update the current display page only if the time has changed.
    if (timeUpdated)
    {
      displayManager.update();
    }

    // Check if settings have changed and need a reload.
    auto &config = ConfigManager::getInstance();
    if (config.isDirty())
    {
      // A "soft" refresh is enough to apply settings changes
      // without blanking the screen.
      displayManager.refresh();
      config.clearDirtyFlag();
      Serial.println("Configuration reloaded and page refreshed.");
    }

    // --- Button Handling Logic ---
    if (buttonReleaseTime > 0)
    {
      unsigned long pressDuration = buttonReleaseTime - buttonPressTime;

      if (alarmManager.isRinging())
      {
        int alarmId = alarmManager.getActiveAlarmId();
        if (alarmId != -1)
        {
          auto &config = ConfigManager::getInstance();
          Alarm alarm = config.getAlarm(alarmId); // Get a copy

          if (pressDuration > 10000) // Long press (> 10 seconds) to dismiss
          {
            Serial.println("Long press detected. Dismissing alarm.");
            alarm.dismiss();
          }
          else // Short press to snooze
          {
            Serial.println("Short press detected. Snoozing alarm.");
            alarm.snooze();
          }
          config.setAlarm(alarmId, alarm); // Save the updated state
          alarmManager.stop();             // Stop the physical ringing
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
      if (config.getAlarm(i).isEnabled())
      {
        anyAlarmEnabled = true;
        break;
      }
    }
    displayManager.drawAlarmIcon(anyAlarmEnabled);
  }
}