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
#include "SerialLog.h"
#include "version.h"

// --- Pin Definitions ---
#define SNOOZE_BUTTON_PIN 5
#define BOOT_BUTTON_PIN 0
#define LOOP_INTERVAL 100 // Interval for the main loop in milliseconds

// --- Global Variables for Timers & Button Handling ---
unsigned long lastLoopTime = 0;
volatile unsigned long pressDuration = 0;
volatile bool newPress = false;
unsigned long bootButtonPressTime = 0;

// --- Interrupt Service Routine (ISR) ---
void IRAM_ATTR handleButtonInterrupt()
{
  static unsigned long lastInterruptTime = 0;
  static unsigned long buttonPressTime = 0;
  unsigned long interruptTime = millis();

  // Basic debouncing
  if (interruptTime - lastInterruptTime < 50)
  {
    return;
  }
  lastInterruptTime = interruptTime;

  if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
  {
    // Button was just pressed
    buttonPressTime = interruptTime;
  }
  else
  {
    // Button was just released
    if (buttonPressTime > 0)
    { // Ensure we have a valid press time
      pressDuration = interruptTime - buttonPressTime;
      newPress = true;
      buttonPressTime = 0; // Reset for next press
    }
  }
}

/**
 * @brief Handles the boot button check for factory reset during runtime.
 */
void handleBootButton()
{
  if (digitalRead(BOOT_BUTTON_PIN) == LOW)
  {
    if (bootButtonPressTime == 0)
    {
      // Button was just pressed
      bootButtonPressTime = millis();
      SerialLog::getInstance().print("Boot button pressed. Timer started for factory reset...\n");
    }
    else if (millis() - bootButtonPressTime > 60000)
    {
      // Button has been held for 60 seconds
      SerialLog::getInstance().print("Factory reset triggered by boot button.\n");
      ConfigManager::getInstance().factoryReset();
      ESP.restart();
    }
  }
  else
  {
    if (bootButtonPressTime > 0)
    {
      // Button was released before the 60-second mark
      SerialLog::getInstance().print("Boot button released. Factory reset cancelled.\n");
      bootButtonPressTime = 0;
    }
  }
}

/**
 * @brief The main setup function, run once on boot.
 */
void setup()
{
  Serial.begin(115200);
  auto &logger = SerialLog::getInstance();

  // Disable logging if the version string does not contain "dev"
  if (String(FIRMWARE_VERSION).indexOf("dev") == -1)
  {
    logger.setLoggingEnabled(false);
  }

  logger.print("\n\n--- ESP32 Clock Booting Up ---\n");

  // Initialize managers and hardware.
  logger.print("Initializing ConfigManager...\n");
  ConfigManager::getInstance().begin();

  // Initialize the snooze button
  logger.print("Initializing Snooze Button...\n");
  pinMode(SNOOZE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN), handleButtonInterrupt, CHANGE);

  // Initialize the Alarm Manager
  logger.print("Initializing AlarmManager...\n");
  AlarmManager::getInstance().begin();

  auto &display = Display::getInstance();
  logger.print("Initializing Display...\n");
  display.begin();

  logger.print("Initializing Sensors...\n");
  setupSensors();
  display.drawStatusMessage("Initializing...");

  // Initialize WiFi. This will connect or start an AP.
  // The begin method returns true if it started the captive portal.
  logger.print("Initializing WiFiManager...\n");
  bool captivePortalStarted = WiFiManager::getInstance().begin();

  // If captive portal is active, enable it on the web server.
  if (captivePortalStarted)
  {
    logger.print("Captive Portal is active. Enabling on web server.\n");
    ClockWebServer::getInstance().enableCaptivePortal();
  }

  // Start the web server. It will now have the correct routes.
  logger.print("Starting Web Server...\n");
  ClockWebServer::getInstance().begin();

  // Add a small delay for web server to stabilize before mDNS announcement.
  delay(100);

  // Initialize the Display Manager
  logger.print("Initializing DisplayManager...\n");
  auto &displayManager = DisplayManager::getInstance();
  displayManager.begin(display.getTft());

  // Add pages to the manager. Ownership is moved to the manager.
  logger.print("Adding pages to DisplayManager...\n");
  displayManager.addPage(std::make_unique<ClockPage>(&display.getTft()));
  displayManager.addPage(std::make_unique<InfoPage>());

  // --- Post-WiFi Initialization Logic ---
  auto &timeManager = TimeManager::getInstance();

  // If the captive portal was started, the device needs configuration.
  // This takes precedence over any other mode.
  if (captivePortalStarted)
  {
    logger.print("Captive portal is active. Displaying setup instructions.\n");
    display.drawMultiLineStatusMessage("Connect to Clock-Setup", "Go to http://192.168.4.1");
  }
  // If not in captive portal mode, check for a successful connection.
  else if (WiFiManager::getInstance().isConnected())
  {
    logger.print("WiFi connected. Syncing time...\n");
    display.drawStatusMessage("Syncing Time...");
    // Log RTC time validity before syncing
    if (timeManager.isTimeSet())
    {
      logger.print("RTC time is valid.\n");
    }
    else
    {
      logger.print("RTC time is not set or invalid.\n");
    }
    timeManager.begin(); // This will perform the initial NTP sync.
    displayManager.setPage(0);
  }
  // If no captive portal and no connection, it's a network outage.
  // Fall back to RTC if the time is valid.
  else if (timeManager.isTimeSet())
  {
    logger.print("WiFi connection failed. RTC time is valid. Starting in offline mode.\n");
    display.drawMultiLineStatusMessage("Offline Mode", "AP: Clock-Setup");
    delay(5000); // Show the message for 5 seconds.
    displayManager.setPage(0);
  }
  // This case should rarely be hit, but as a fallback, show setup.
  else
  {
    logger.print("WiFi connection failed and RTC not set. Displaying setup instructions.\n");
    display.drawMultiLineStatusMessage("Connect to Clock-Setup", "Go to http://192.168.4.1");
  }

  logger.print("--- Setup Complete ---\n");
}

/**
 * @brief The main application loop.
 */
void loop()
{
  // Handle DNS requests for the captive portal if it's active.
  auto &wifiManager = WiFiManager::getInstance();
  wifiManager.handleDns();
  // Manage WiFi connection and reconnection.
  wifiManager.handleConnection();

  // Implement a non-blocking delay to prevent watchdog timeouts.
  unsigned long currentMillis = millis();
  if (currentMillis - lastLoopTime < LOOP_INTERVAL)
  {
    delay(1); // Yield to other tasks.
    return;
  }
  lastLoopTime = currentMillis;

  // --- Core Clock Logic (Runs regardless of WiFi connection) ---
  auto &timeManager = TimeManager::getInstance();
  auto &display = Display::getInstance();
  auto &displayManager = DisplayManager::getInstance();
  auto &alarmManager = AlarmManager::getInstance();
  auto &config = ConfigManager::getInstance();

  // Perform periodic tasks that don't require WiFi.
  alarmManager.update();
  bool timeUpdated = timeManager.update(); // Updates time from the RTC
  timeManager.checkAlarms();
  display.updateBrightness();
  handleSensorUpdates();

  // Update the current display page if the time has changed.
  if (timeUpdated)
  {
    displayManager.update();
  }

  // Check if settings have changed and need a reload.
  if (config.isDirty())
  {
    displayManager.refresh();
    config.clearDirtyFlag();
    SerialLog::getInstance().print("Settings changed, refreshing display.\n");
  }

  // --- Button Handling Logic (Runs regardless of WiFi connection) ---
  if (newPress)
  {
    unsigned long duration;
    // Atomically read and reset the flag
    noInterrupts();
    duration = pressDuration;
    newPress = false;
    interrupts();

    SerialLog::getInstance().printf("Button press detected. Duration: %lu ms\n", duration);

    if (alarmManager.isRinging())
    {
      int alarmId = alarmManager.getActiveAlarmId();
      if (alarmId != -1)
      {
        Alarm alarm = config.getAlarm(alarmId); // Get a copy
        bool isSnooze = duration <= 10000;

        if (isSnooze)
        {
          alarm.snooze();
          DateTime now = TimeManager::getInstance().getRTCTime();
          DateTime snoozeUntil = now + TimeSpan(SNOOZE_DURATION_MS / 1000);
          SerialLog::getInstance().printf("Snoozing until: %02d:%02d:%02d\n", snoozeUntil.hour(), snoozeUntil.minute(), snoozeUntil.second());
        }
        else // Long press (> 10 seconds) to dismiss
        {
          SerialLog::getInstance().print("Long press detected. Dismissing alarm.\n");
          alarm.dismiss();
        }
        config.setAlarm(alarmId, alarm); // Save the updated state
        config.save();                   // Persist the change
        alarmManager.stop();             // Stop the ringing
      }
    }
    else
    {
      // If not ringing, a short press cycles pages.
      int newIndex = (displayManager.getCurrentPageIndex() + 1) % displayManager.getPagesSize();
      SerialLog::getInstance().printf("Cycling to page index: %d\n", newIndex);
      displayManager.setPage(newIndex);
    }
  }

  // --- Update Alarm Icon (Runs regardless of WiFi connection) ---
  bool anyAlarmEnabled = false;
  bool anyAlarmSnoozed = false;
  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    const auto &alarm = config.getAlarm(i);
    if (alarm.isEnabled())
    {
      anyAlarmEnabled = true;
      if (alarm.isSnoozed())
      {
        anyAlarmSnoozed = true;
      }
    }
  }
  displayManager.drawAlarmIcon(anyAlarmEnabled, anyAlarmSnoozed);

  // --- WiFi-Dependent Logic ---
  if (WiFiManager::getInstance().isConnected())
  {
    // Only attempt to sync time with the internet if connected.
    timeManager.updateNtp();
    timeManager.checkDailySync();
  }

  // --- Handle Boot Button for Factory Reset (Runs regardless of WiFi connection) ---
  handleBootButton();
}