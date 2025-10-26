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
#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

// --- Pin Definitions ---
#define SNOOZE_BUTTON_PIN 5
#define BOOT_BUTTON_PIN 0
#define LOOP_INTERVAL 100 // Interval for the main loop in milliseconds

// --- Constants for Button Logic ---
constexpr unsigned long DISMISS_HOLD_DURATION_MS = 3000;

// --- Global Variables for Timers & Button Handling ---
unsigned long lastLoopTime = 0;
volatile unsigned long pressDuration = 0;
volatile bool newPress = false;
unsigned long bootButtonPressTime = 0;

// --- Button State for Alarm Handling ---
static unsigned long alarmButtonPressTime = 0;
static bool actionTaken = false;

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
 * @brief Triggers a factory reset, logs the source, and reboots the device.
 * @param source A string indicating what triggered the reset (e.g., "snooze button").
 * @param needsConfigInit A boolean indicating if the ConfigManager needs to be initialized.
 *                        This is true for boot-time resets.
 */
void triggerFactoryReset(const char *source, bool needsConfigInit)
{
  auto &logger = SerialLog::getInstance();
  logger.printf("Factory reset triggered by %s.\n", source);

  // Attempt to show a message on the display.
  Display::getInstance().drawStatusMessage("Resetting...");

  // For boot-time resets, the ConfigManager must be initialized first.
  if (needsConfigInit)
  {
    ConfigManager::getInstance().begin();
  }

  ConfigManager::getInstance().factoryReset();
  ESP.restart(); // The device will restart into a clean state.
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
    else if (millis() - bootButtonPressTime > 30000)
    {
      // Button has been held for 30 seconds
      triggerFactoryReset("boot button", false);
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
  pinMode(SNOOZE_BUTTON_PIN, INPUT_PULLUP); // Use Snooze button for boot-time reset
  auto &logger = SerialLog::getInstance();
  bool displayInitialized = false;

  // Check for factory reset request at boot using the Snooze button.
  // This avoids conflicts with the ESP32's hardware bootloader mode.
  if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
  {
    auto &display = Display::getInstance();
    display.begin(); // Initialize display only when needed.
    displayInitialized = true;

    logger.print("Snooze button held. Checking for factory reset...\n");
    display.drawStatusMessage("Hold for factory reset");

    unsigned long pressStartTime = millis();
    while (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
    {
      // After 10 seconds, trigger the reset.
      if (millis() - pressStartTime > 10000)
      {
        triggerFactoryReset("snooze button", true);
      }
      delay(50); // Small delay to prevent busy-waiting.
    }

    // If the button was released before the 10-second mark, cancel and proceed.
    logger.print("Snooze button released. Factory reset cancelled.\n");
    display.drawStatusMessage("Reset cancelled");
    delay(2000);
  }

  // --- Proceed with the normal boot sequence ---
  auto &display = Display::getInstance();
  if (!displayInitialized)
  {
    display.begin();
  }
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP); // BOOT button is used for run-time reset

  // Disable logging if the version string does not contain "dev"
  if (String(FIRMWARE_VERSION).indexOf("dev") == -1)
  {
    logger.setLoggingEnabled(false);
  }

  logger.print("\n\n--- ESP32 Clock Booting Up ---\n");

  // Initialize managers and hardware.
  logger.print("Initializing ConfigManager...\n");
  ConfigManager::getInstance().begin();

  // Initialize the snooze button interrupt
  logger.print("Initializing Snooze Button...\n");
  attachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN), handleButtonInterrupt, CHANGE);

  // Initialize the Alarm Manager
  logger.print("Initializing AlarmManager...\n");
  AlarmManager::getInstance().begin();

  auto &displayManager = DisplayManager::getInstance();
  logger.print("Initializing DisplayManager...\n");
  displayManager.begin(display.getTft());
  display.drawStatusMessage("Initializing...");

  logger.print("Initializing Sensors...\n");
  setupSensors();

  // --- Critical Hardware Checks ---
  if (!isRtcFound())
  {
    logger.print("CRITICAL: RTC module not found. Halting execution.\n");
    displayManager.showErrorScreen("RTC MODULE NOT FOUND");
    while (1)
    {
      delay(1000);
    }
  }

  // Initialize WiFi. This will connect or start an AP.
  logger.print("Initializing WiFiManager...\n");
  bool captivePortalStarted = WiFiManager::getInstance().begin();

  // If captive portal is active, enable it on the web server.
  if (captivePortalStarted)
  {
    logger.print("Captive Portal is active. Enabling on web server.\n");
    ClockWebServer::getInstance().enableCaptivePortal();
  }

  // Start the web server.
  logger.print("Starting Web Server...\n");
  ClockWebServer::getInstance().begin();

  delay(100); // Delay for web server stabilization.

  // Add pages to the manager.
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
  timeManager.updateSnoozeStates();
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

  // --- State-based Interrupt Management ---
  // We use a state variable to detect the transition to/from ringing,
  // allowing us to detach the interrupt only once when the alarm starts
  // and re-attach it only once when it stops.
  static bool alarmWasRinging = false;
  bool alarmIsRinging = alarmManager.isRinging();

  if (alarmIsRinging && !alarmWasRinging)
  {
    // Alarm has just started: detach the page-cycling interrupt
    // so it doesn't conflict with the polling logic for snooze/dismiss.
    detachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN));
    newPress = false; // Clear any pending button press from the interrupt
    SerialLog::getInstance().print("Alarm started. Interrupt detached.\n");
  }
  else if (!alarmIsRinging && alarmWasRinging)
  {
    // Alarm has just stopped: re-attach the interrupt for normal operation.
    attachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN), handleButtonInterrupt, CHANGE);
    SerialLog::getInstance().print("Alarm stopped. Interrupt re-attached.\n");
  }
  alarmWasRinging = alarmIsRinging; // Update state for the next loop

  // --- Button Handling Logic (Switches between polling and interrupts) ---
  if (alarmIsRinging)
  {
    // --- Polling-based logic for when an alarm is active ---
    if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
    {
      // Button is currently pressed
      if (alarmButtonPressTime == 0)
      {
        // Button was just pressed
        alarmButtonPressTime = currentMillis;
        actionTaken = false;
        SerialLog::getInstance().print("Alarm active: Button press detected.\n");
      }
      else if (!actionTaken)
      {
        // Update the progress bar while the button is held
        float progress = (float)(currentMillis - alarmButtonPressTime) / DISMISS_HOLD_DURATION_MS;
        displayManager.drawDismissProgressBar(progress);
      }

      if (!actionTaken && currentMillis - alarmButtonPressTime > DISMISS_HOLD_DURATION_MS)
      {
        // Button has been held long enough, dismiss the alarm
        SerialLog::getInstance().print("Alarm active: Button held. Dismissing.\n");
        int alarmId = alarmManager.getActiveAlarmId();
        if (alarmId != -1)
        {
          Alarm alarm = config.getAlarm(alarmId);
          alarm.dismiss(timeManager.getRTCTime());
          config.setAlarm(alarmId, alarm);
          config.save();
          alarmManager.stop();
          displayManager.clearAlarmOverlay();
        }
        actionTaken = true; // Ensure dismiss is only called once
      }
    }
    else
    {
      // Button is not pressed (it's released)
      if (alarmButtonPressTime > 0 && !actionTaken)
      {
        // Button was released before the 3-second dismiss
        SerialLog::getInstance().print("Alarm active: Button released. Snoozing.\n");
        int alarmId = alarmManager.getActiveAlarmId();
        if (alarmId != -1)
        {
          Alarm alarm = config.getAlarm(alarmId);
          alarm.snooze();
          config.setAlarm(alarmId, alarm);
          config.save();
          alarmManager.stop();
          displayManager.clearAlarmOverlay();
        }
      }
      // Reset for the next press
      alarmButtonPressTime = 0;
    }
  }
  else if (newPress)
  {
    // --- Interrupt-based logic for normal operations (page cycling) ---
    // This code only runs when the alarm is not ringing, because the
    // interrupt that sets `newPress` is detached during an alarm.
    unsigned long duration;
    noInterrupts();
    duration = pressDuration;
    newPress = false;
    interrupts();

    SerialLog::getInstance().printf("Button press detected. Duration: %lu ms\n", duration);
    // A short press cycles pages.
    int newIndex = (displayManager.getCurrentPageIndex() + 1) % displayManager.getPagesSize();
    SerialLog::getInstance().printf("Cycling to page index: %d\n", newIndex);
    displayManager.setPage(newIndex);
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