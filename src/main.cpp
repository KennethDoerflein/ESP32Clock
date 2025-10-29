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
#include <memory> // For std::unique_ptr
#include <ESPmDNS.h>

#include "constants.h"
// Include all custom module headers.
#include "ConfigManager.h"
#include "AlarmManager.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "WiFiManager.h"
#include "DisplayManager.h"
#include "pages/ClockPage.h"
#include "pages/InfoPage.h"
#include "ClockWebServer.h"
#include "SerialLog.h"
#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

#ifdef USE_RTC_ALARMS
volatile bool g_alarm_triggered = false;
#endif

// --- Global Variables for Timers & Button Handling ---
unsigned long g_lastLoopTime = 0;
volatile unsigned long g_pressDuration = 0;
volatile bool g_newPress = false;
unsigned long g_bootButtonPressTime = 0;

// --- Button State for Alarm Handling ---
static unsigned long s_alarmButtonPressTime = 0;
static bool s_actionTaken = false;

// --- Interrupt Service Routine (ISR) ---
void IRAM_ATTR handleButtonInterrupt()
{
  static unsigned long s_lastInterruptTime = 0;
  static unsigned long s_buttonPressTime = 0;
  unsigned long interruptTime = millis();

  // Basic debouncing
  if (interruptTime - s_lastInterruptTime < DEBOUNCE_DELAY)
  {
    return;
  }
  s_lastInterruptTime = interruptTime;

  if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
  {
    // Button was just pressed
    s_buttonPressTime = interruptTime;
  }
  else
  {
    // Button was just released
    if (s_buttonPressTime > 0)
    { // Ensure we have a valid press time
      g_pressDuration = interruptTime - s_buttonPressTime;
      g_newPress = true;
      s_buttonPressTime = 0; // Reset for next press
    }
  }
}

#ifdef USE_RTC_ALARMS
// --- RTC Alarm ISR ---
void IRAM_ATTR onAlarm()
{
  g_alarm_triggered = true;
}
#endif

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
    if (g_bootButtonPressTime == 0)
    {
      // Button was just pressed
      g_bootButtonPressTime = millis();
      SerialLog::getInstance().print("Boot button pressed. Timer started for factory reset...\n");
    }
    else if (millis() - g_bootButtonPressTime > FACTORY_RESET_HOLD_TIME)
    {
      // Button has been held for 30 seconds
      triggerFactoryReset("boot button", false);
    }
  }
  else
  {
    if (g_bootButtonPressTime > 0)
    {
      // Button was released before the 60-second mark
      SerialLog::getInstance().print("Boot button released. Factory reset cancelled.\n");
      g_bootButtonPressTime = 0;
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
      if (millis() - pressStartTime > BOOT_FACTORY_RESET_HOLD_TIME)
      {
        triggerFactoryReset("snooze button", true);
      }
      delay(DEBOUNCE_DELAY); // Small delay to prevent busy-waiting.
    }

    // If the button was released before the 10-second mark, cancel and proceed.
    logger.print("Snooze button released. Factory reset cancelled.\n");
    display.drawStatusMessage("Reset cancelled");
    delay(SETUP_CANCEL_DELAY);
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

#ifdef USE_RTC_ALARMS
  // Initialize the RTC alarm interrupt
  logger.print("Initializing RTC Interrupt...\n");
  pinMode(RTC_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), onAlarm, FALLING);
#endif

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

  delay(WEB_SERVER_STABILIZATION_DELAY); // Delay for web server stabilization.

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
    delay(OFFLINE_MODE_MESSAGE_DELAY); // Show the message for 5 seconds.
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
  if (currentMillis - g_lastLoopTime < LOOP_INTERVAL)
  {
    delay(1); // Yield to other tasks.
    return;
  }
  g_lastLoopTime = currentMillis;

  // --- Core Clock Logic (Runs regardless of WiFi connection) ---
  auto &timeManager = TimeManager::getInstance();
  auto &display = Display::getInstance();
  auto &displayManager = DisplayManager::getInstance();
  auto &alarmManager = AlarmManager::getInstance();
  auto &config = ConfigManager::getInstance();

  // Perform periodic tasks that don't require WiFi.
  alarmManager.update();
  bool timeUpdated = timeManager.update(); // Updates time from the RTC
#ifdef USE_RTC_ALARMS
  if (g_alarm_triggered)
  {
    g_alarm_triggered = false;
    timeManager.handleAlarm();
  }
#else
  timeManager.checkAlarms();
#endif
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
#ifdef USE_RTC_ALARMS
    timeManager.setNextAlarms();
#endif
    config.clearDirtyFlag();
    SerialLog::getInstance().print("Settings changed, refreshing display.\n");
  }

  // --- State-based Interrupt Management ---
  // We use a state variable to detect the transition to/from ringing or snoozing,
  // allowing us to detach the interrupt only once when an alarm-related
  // action is possible, and re-attach it when it's not.
  static bool s_wasInAlarmOrSnoozeState = false;
  bool isRinging = alarmManager.isRinging();
  bool isSnoozed = false;
  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    if (config.getAlarm(i).isSnoozed())
    {
      isSnoozed = true;
      break;
    }
  }
  bool isInAlarmOrSnoozeState = isRinging || isSnoozed;

  if (isInAlarmOrSnoozeState && !s_wasInAlarmOrSnoozeState)
  {
    // An alarm is ringing or snoozed: detach the page-cycling interrupt
    // so it doesn't conflict with the polling logic for snooze/dismiss.
    detachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN));
    g_newPress = false; // Clear any pending button press from the interrupt
    SerialLog::getInstance().print("Alarm/Snooze state entered. Interrupt detached.\n");
  }
  else if (!isInAlarmOrSnoozeState && s_wasInAlarmOrSnoozeState)
  {
    // Alarm has stopped and no alarm is snoozed: re-attach the interrupt for normal operation.
    attachInterrupt(digitalPinToInterrupt(SNOOZE_BUTTON_PIN), handleButtonInterrupt, CHANGE);
    SerialLog::getInstance().print("Alarm/Snooze state exited. Interrupt re-attached.\n");
  }
  s_wasInAlarmOrSnoozeState = isInAlarmOrSnoozeState; // Update state for the next loop

  // --- Button Handling Logic (Switches between polling and interrupts) ---
  if (isRinging)
  {
    // --- Polling-based logic for when an alarm is active ---
    if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
    {
      // Button is currently pressed
      if (s_alarmButtonPressTime == 0)
      {
        // Button was just pressed
        s_alarmButtonPressTime = currentMillis;
        s_actionTaken = false;
        SerialLog::getInstance().print("Alarm active: Button press detected.\n");
      }
      else if (!s_actionTaken)
      {
        // Update the progress bar while the button is held
        unsigned long dismissDurationMs = config.getDismissDuration() * 1000;
        float progress = (float)(currentMillis - s_alarmButtonPressTime) / dismissDurationMs;
        if (displayManager.getCurrentPageIndex() == 0)
        {
          static_cast<ClockPage *>(displayManager.getCurrentPage())->setDismissProgress(progress);
          displayManager.update();
        }
      }

      if (!s_actionTaken && currentMillis - s_alarmButtonPressTime > (config.getDismissDuration() * 1000))
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
          if (displayManager.getCurrentPageIndex() == 0)
          {
            // Reset the progress bar to 0 and then force a full render update
            // to show the new snooze state and ensure the progress bar is cleared.
            static_cast<ClockPage *>(displayManager.getCurrentPage())->setDismissProgress(0.0f);
            displayManager.update();
          }
        }
        s_actionTaken = true; // Ensure dismiss is only called once
      }
    }
    else
    {
      // Button is not pressed (it's released)
      if (s_alarmButtonPressTime > 0 && !s_actionTaken)
      {
        // Button was released before the dismiss action, so snooze.
        SerialLog::getInstance().print("Alarm active: Button released. Snoozing.\n");
        int alarmId = alarmManager.getActiveAlarmId();
        if (alarmId != -1)
        {
          Alarm alarm = config.getAlarm(alarmId);
          alarm.snooze(config.getSnoozeDuration());
          config.setAlarm(alarmId, alarm);
          config.save();
          alarmManager.stop();
          if (displayManager.getCurrentPageIndex() == 0)
          {
            // Reset the progress bar and then force a full render update
            // to show the new snooze state and ensure the progress bar is cleared.
            static_cast<ClockPage *>(displayManager.getCurrentPage())->setDismissProgress(0.0f);
            displayManager.update();
          }
        }
      }
      // Always reset the button timer and action flag on release.
      s_alarmButtonPressTime = 0;
      s_actionTaken = false;
    }
  }
  else if (isSnoozed)
  {
    // --- Polling-based logic for when an alarm is snoozed ---
    if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
    {
      // Button is currently pressed
      if (s_alarmButtonPressTime == 0)
      {
        // Button was just pressed
        s_alarmButtonPressTime = currentMillis;
        s_actionTaken = false;
        SerialLog::getInstance().print("Snooze active: Button press detected.\n");
      }
      else if (!s_actionTaken)
      {
        // Update the progress bar while the button is held
        float progress = (float)(currentMillis - s_alarmButtonPressTime) / 3000.0f;
        if (displayManager.getCurrentPageIndex() == 0)
        {
          static_cast<ClockPage *>(displayManager.getCurrentPage())->setDismissProgress(progress);
          displayManager.update();
        }
      }

      // If the button is held long enough, end the snooze for all snoozed alarms
      if (!s_actionTaken && currentMillis - s_alarmButtonPressTime > SNOOZE_DISMISS_HOLD_TIME) // 3-second hold
      {
        SerialLog::getInstance().print("Snooze active: Button held. Ending snooze.\n");
        for (int i = 0; i < config.getNumAlarms(); ++i)
        {
          Alarm alarm = config.getAlarm(i);
          if (alarm.isSnoozed())
          {
            alarm.dismiss(timeManager.getRTCTime());
            config.setAlarm(i, alarm);
          }
        }
        config.save();
        if (displayManager.getCurrentPageIndex() == 0)
        {
          // Force the alarm sprite to re-render, which will now be empty
          static_cast<ClockPage *>(displayManager.getCurrentPage())->updateAlarmSprite();
        }
        s_actionTaken = true; // Ensure action is only called once
      }
    }
    else
    {
      // Button is not pressed (it's released)
      if (s_alarmButtonPressTime > 0)
      {
        // If the button was being held, reset the progress bar and timer.
        if (displayManager.getCurrentPageIndex() == 0)
        {
          static_cast<ClockPage *>(displayManager.getCurrentPage())->setDismissProgress(0.0f);
          displayManager.update();
        }
        s_alarmButtonPressTime = 0;
      }
    }
  }
  else if (g_newPress)
  {
    // --- Interrupt-based logic for normal operations (page cycling) ---
    // This code only runs when no alarm is ringing or snoozed, because the
    // interrupt that sets `g_newPress` is detached during those states.
    unsigned long duration;
    noInterrupts();
    duration = g_pressDuration;
    g_newPress = false;
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
    timeManager.checkDriftAndResync();
  }

  // --- Handle Boot Button for Factory Reset (Runs regardless of WiFi connection) ---
  handleBootButton();
}