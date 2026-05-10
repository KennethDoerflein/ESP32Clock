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
#include <Preferences.h>

#include "Constants.h"
// Include all custom module headers.
#include "ConfigManager.h"
#include "AlarmManager.h"
#include "SensorModule.h"
#include "Display.h"
#include "TimeManager.h"
#include "WiFiManager.h"
#include "DisplayManager.h"
#include "pages/ClockPage.h"
#include "pages/InfoPage.h"
#include "pages/WeatherPage.h"
#include "pages/WeatherClockPage.h"
#include "ClockWebServer.h"
#include "SerialLog.h"
#include <LittleFS.h>
#include "ButtonManager.h"
#include "WeatherService.h"
#include "UpdateManager.h"
#include "WebContent.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_task_wdt.h>
#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

volatile bool g_alarm_triggered = false;

// --- Global Variables for Timers & Button Handling ---
unsigned long g_lastLoopTime = 0;
unsigned long g_bootButtonPressTime = 0;

// --- Button State for Alarm Handling ---
static unsigned long s_alarmButtonPressTime = 0;
static bool s_actionTaken = false;

enum AlarmState
{
  IDLE,
  RINGING,
  SNOOZED
};
AlarmState g_alarmState = IDLE;

ButtonManager snoozeButton(SNOOZE_BUTTON_PIN);

// --- Task Handles ---
TaskHandle_t g_logicTaskHandle = NULL;

/**
 * @brief Updates the global alarm state based on the current system status.
 *
 * This function centralizes the logic for determining whether the system
 * should be in the IDLE, RINGING, or SNOOZED state. It checks the
 * AlarmManager and the configuration to make the decision.
 */
void updateAlarmState(const std::vector<Alarm> &alarms)
{
  auto &alarmManager = AlarmManager::getInstance();
  AlarmState oldState = g_alarmState;

  // Determine the new state based on current conditions.
  // The order of checks is important: ringing takes precedence over snoozed.
  AlarmState newState;
  if (alarmManager.isRinging())
  {
    newState = RINGING;
  }
  else
  {
    bool anySnoozed = false;
    for (const auto &alarm : alarms)
    {
      if (alarm.isSnoozed())
      {
        anySnoozed = true;
        break;
      }
    }
    newState = anySnoozed ? SNOOZED : IDLE;
  }

  // --- Handle State Transitions ---
  if (newState != oldState)
  {
    g_alarmState = newState; // Update the global state
    SerialLog::getInstance().printf("Alarm state changed from %d to %d\n", oldState, newState);

    // When moving to a state that requires polling, detach the interrupt.
    // When returning to IDLE, re-attach it.
    if (newState == IDLE)
    {
      snoozeButton.attach();
    }
    else
    {
      snoozeButton.detach();
      snoozeButton.clearNewPress(); // Ensure no stale presses are carried over
    }
  }
}

// --- RTC Alarm ISR ---
void IRAM_ATTR onAlarm()
{
  g_alarm_triggered = true;
}

/**
 * @brief Resets the safe mode boot counter to zero.
 *
 * Called after a successful setup to indicate the firmware booted cleanly,
 * and also from safe mode after a successful firmware upload.
 */
void resetBootCounter()
{
  Preferences prefs;
  prefs.begin(SAFE_MODE_NVS_NAMESPACE, false);
  prefs.putUInt(SAFE_MODE_NVS_KEY, 0);
  prefs.end();
}

/**
 * @brief Enters safe mode due to a detected boot loop.
 *
 * This function never returns. It initializes only the minimal set of
 * peripherals needed to allow firmware upload over WiFi:
 * - Display (for status messages)
 * - ConfigManager (read-only, for WiFi credentials)
 * - WiFi (STA with saved credentials, or AP fallback)
 * - A minimal web server with OTA upload capability
 *
 * It deliberately avoids initializing sensors, alarms, time management,
 * or the full web server to prevent re-triggering whatever crash caused
 * the boot loop.
 */
void enterSafeMode()
{
  auto &logger = SerialLog::getInstance();
  logger.print("\n*** ENTERING SAFE MODE ***\n");
  logger.print("Boot loop detected. Starting minimal recovery environment.\n");

  // Initialize display
  auto &display = Display::getInstance();
  display.begin();
  display.drawMultiLineStatusMessage("SAFE MODE", "Starting WiFi...");

  // Initialize ConfigManager to read saved WiFi credentials
  ConfigManager::getInstance().begin();

  // Attempt WiFi STA connection with saved credentials
  String ssid = ConfigManager::getInstance().getWifiSSID();
  String password = ConfigManager::getInstance().getWifiPassword();
  bool wifiConnected = false;
  DNSServer *dnsServer = nullptr;

  if (ssid.length() > 0)
  {
    logger.printf("Safe Mode: Attempting to connect to SSID: %s\n", ssid.c_str());
    display.drawMultiLineStatusMessage("SAFE MODE", "Connecting WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < SAFE_MODE_WIFI_TIMEOUT)
    {
      delay(250);
      logger.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      wifiConnected = true;
      logger.printf("\nSafe Mode: WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
      display.drawMultiLineStatusMessage("SAFE MODE", ("IP: " + WiFi.localIP().toString()).c_str());
    }
    else
    {
      logger.print("\nSafe Mode: WiFi connection failed.\n");
      WiFi.disconnect(true);
    }
  }

  // If STA failed, start AP
  if (!wifiConnected)
  {
    logger.printf("Safe Mode: Starting AP '%s'\n", SAFE_MODE_AP_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SAFE_MODE_AP_SSID);
    IPAddress apIP = WiFi.softAPIP();
    logger.printf("Safe Mode: AP IP: %s\n", apIP.toString().c_str());
    display.drawMultiLineStatusMessage("SAFE MODE", ("AP: " + String(SAFE_MODE_AP_SSID)).c_str());

    // Start DNS server for captive portal behavior
    dnsServer = new DNSServer();
    dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer->start(53, "*", apIP);
  }

  // Create a minimal web server (separate from ClockWebServer)
  AsyncWebServer safeServer(80);

  // Serve the safe mode HTML page
  safeServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send_P(200, "text/html", SAFE_MODE_HTML); });

  // Handle firmware upload (reuses UpdateManager)
  safeServer.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest *request)
      {
        // Response is sent in the upload handler's final callback
      },
      [](AsyncWebServerRequest *request, String filename, size_t index,
         uint8_t *data, size_t len, bool final)
      {
        if (UpdateManager::getInstance().isUpdateInProgress() && index == 0)
        {
          request->send(409, "text/plain", "An update is already in progress.");
          return;
        }
        UpdateManager::getInstance().handleFileUpload(data, len, index, request->contentLength());
        if (final)
        {
          if (UpdateManager::getInstance().endUpdate())
          {
            // Reset boot counter so the new firmware boots normally
            resetBootCounter();
            request->send(200, "text/plain", "Update successful! Rebooting...");
            delay(1000);
            ESP.restart();
          }
          else
          {
            String err = UpdateManager::getInstance().getLastError();
            request->send(500, "text/plain", "Update failed: " + (err.length() > 0 ? err : String("Unknown error.")));
          }
        }
      });

  // Reboot endpoint
  safeServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
                {
    request->send(200, "text/plain", "Rebooting...");
    request->onDisconnect([](){ ESP.restart(); }); });

  // Catch-all for captive portal redirect (AP mode only)
  if (!wifiConnected)
  {
    safeServer.onNotFound([](AsyncWebServerRequest *request)
                          {
      String redirectUrl = "http://" + request->host();
      request->redirect(redirectUrl); });
  }

  safeServer.begin();
  logger.print("Safe Mode: Web server started. Waiting for firmware upload.\n");

  // Infinite loop — safe mode never exits except via reboot/reflash
  for (;;)
  {
    if (dnsServer)
    {
      dnsServer->processNextRequest();
    }
    esp_task_wdt_reset();
    delay(10);
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
 * @brief The background logic task running on Core 0.
 *
 * Handles network connectivity, time synchronization, weather updates,
 * and configuration saving.
 */
void logicTask(void *pvParameters)
{
  SerialLog::getInstance().print("Logic Task started on Core 0\n");
  esp_task_wdt_add(NULL); // Add this task to the WDT

  auto &wifiManager = WiFiManager::getInstance();
  auto &timeManager = TimeManager::getInstance();
  auto &config = ConfigManager::getInstance();
  auto &weatherService = WeatherService::getInstance();

  for (;;)
  {
    // Handle WiFi
    wifiManager.handleDns();
    wifiManager.handleConnection();

    if (!UpdateManager::getInstance().isUpdateInProgress())
    {
      config.loop();
      weatherService.loop();
    }

    if (!UpdateManager::getInstance().isUpdateInProgress() && wifiManager.isConnected())
    {
      timeManager.updateNtp();
      timeManager.checkDailySync();
      timeManager.checkDriftAndResync();
    }

    // Handle Serial Log (WebSocket updates etc)
    SerialLog::getInstance().loop();

    // Log heap every minute to track stability and fragmentation
    static unsigned long lastHeapLog = 0;
    if (millis() - lastHeapLog > 60000)
    {
      SerialLog::getInstance().printf(
          "Logic Task Heartbeat - Free: %u | Min: %u | MaxAlloc: %u\n",
          ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
      lastHeapLog = millis();
    }

    // Yield to prevent watchdog triggers
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_task_wdt_reset(); // Feed the watchdog
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

  // Initialize LittleFS
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Mount Failed");
  }
  else
  {
    Serial.println("LittleFS Mounted Successfully");
  }

  // Log the reset reason immediately after FS mount
  logger.logResetReason();

  // Initialize Task Watchdog Timer (TWDT)
  // 30 seconds timeout, true = panic (reset) on timeout
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL); // Add the main task (Core 1) to the WDT

  // --- Boot Loop Detection ---
  // Increment a persistent boot counter BEFORE any complex initialization.
  // If we reach the threshold, enter safe mode. The counter is reset to 0
  // at the end of setup() after a successful boot.
  {
    Preferences prefs;
    prefs.begin(SAFE_MODE_NVS_NAMESPACE, false);
    uint32_t bootCount = prefs.getUInt(SAFE_MODE_NVS_KEY, 0);
    bootCount++;
    prefs.putUInt(SAFE_MODE_NVS_KEY, bootCount);
    prefs.end();

    logger.printf("Boot counter: %u / %d\n", bootCount, SAFE_MODE_BOOT_THRESHOLD);

    if (bootCount >= SAFE_MODE_BOOT_THRESHOLD)
    {
      // Boot loop detected — enter safe mode (never returns)
      enterSafeMode();
    }
  }

  bool displayInitialized = false;

  // Initialize ConfigManager.
  logger.print("Initializing ConfigManager...\n");
  ConfigManager::getInstance().begin();

  // Check for factory reset request at boot using the Snooze button.
  // This avoids conflicts with the ESP32's hardware bootloader mode.
  if (digitalRead(SNOOZE_BUTTON_PIN) == LOW)
  {
    auto &display = Display::getInstance();
    if (!displayInitialized)
    {
      display.begin(); // Initialize display only when needed.
      displayInitialized = true;
    }

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

  logger.print("\n\n--- ESP32 Clock Booting Up ---\n");

  // Initialize the snooze button interrupt
  logger.print("Initializing Snooze Button...\n");
  snoozeButton.begin();

  // Initialize the RTC alarm interrupt
  logger.print("Initializing RTC Interrupt...\n");
  pinMode(RTC_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), onAlarm, FALLING);

  // Initialize the Alarm Manager
  logger.print("Initializing AlarmManager...\n");
  AlarmManager::getInstance().begin();

  // Initialize Weather Service
  logger.print("Initializing WeatherService...\n");
  WeatherService::getInstance().begin();

  auto &displayManager = DisplayManager::getInstance();
  logger.print("Initializing DisplayManager...\n");
  displayManager.begin(display.getTft());
  display.drawStatusMessage("Initializing...");

  // Add a delay before initializing sensors to allow hardware to stabilize.
  logger.print("Waiting for hardware to stabilize...\n");
  delay(500);

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
  displayManager.addPage(std::make_unique<WeatherPage>());
  displayManager.addPage(std::make_unique<InfoPage>());
  displayManager.addPage(std::make_unique<WeatherClockPage>(&display.getTft()));

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
    displayManager.setPage(ConfigManager::getInstance().getDefaultPage());
  }
  // If no captive portal and no connection, it's a network outage.
  // Fall back to RTC if the time is valid.
  else if (timeManager.isTimeSet())
  {
    logger.print("WiFi connection failed. RTC time is valid. Starting in offline mode.\n");
    display.drawMultiLineStatusMessage("Offline Mode", "AP: Clock-Setup");
    delay(OFFLINE_MODE_MESSAGE_DELAY); // Show the message for 5 seconds.
    displayManager.setPage(ConfigManager::getInstance().getDefaultPage());
  }
  // This case should rarely be hit, but as a fallback, show setup.
  else
  {
    logger.print("WiFi connection failed and RTC not set. Displaying setup instructions.\n");
    display.drawMultiLineStatusMessage("Connect to Clock-Setup", "Go to http://192.168.4.1");
  }

  // Create the Logic Task on Core 0
  xTaskCreatePinnedToCore(
      logicTask,
      "LogicTask",
      12288,
      NULL,
      1,
      &g_logicTaskHandle,
      0 // Core 0
  );

  // Reset the boot counter to 0 — setup completed successfully.
  // This proves the firmware is stable and prevents safe mode on next boot.
  resetBootCounter();
  logger.print("Boot counter reset. Firmware is stable.\n");

  logger.print("--- Setup Complete ---\n");
}

/**
 * @brief The main application loop.
 * Runs on Core 1 by default (Arduino Loop).
 */
void loop()
{
  esp_task_wdt_reset(); // Feed the watchdog

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
  // Note: Config, Weather, WiFi, and SerialLog loops are now in Logic Task (Core 0)

  alarmManager.update();
  bool timeUpdated = timeManager.update(); // Updates time from the RTC
  if (g_alarm_triggered)
  {
    g_alarm_triggered = false;
    timeManager.handleAlarm();
  }
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
    timeManager.setNextAlarms();
    config.clearDirtyFlag();
    SerialLog::getInstance().print("Settings changed, refreshing display.\n");
    // Re-evaluate snooze state and update display
    timeManager.updateSnoozeStates();
    displayManager.update();
  }

  // --- Cache alarm state once per loop to avoid redundant heap copies ---
  std::vector<Alarm> alarms = config.getAllAlarms();

  // --- Alarm State Machine ---
  updateAlarmState(alarms);

  // State actions
  switch (g_alarmState)
  {
  case RINGING:
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
        displayManager.setDismissProgress(progress);
        displayManager.update();
      }

      if (!s_actionTaken && currentMillis - s_alarmButtonPressTime > (config.getDismissDuration() * 1000))
      {
        // Button has been held long enough, dismiss the alarm
        SerialLog::getInstance().print("Alarm active: Button held. Dismissing.\n");
        int alarmId = alarmManager.getActiveAlarmId();
        if (alarmId != -1)
        {
          Alarm alarm = config.getAlarmById(alarmId);
          if (alarm.getId() != 255)
          {
            alarm.dismiss(timeManager.getCachedTime());
            config.setAlarmById(alarmId, alarm);
            config.save();
          }
          alarmManager.stop();

          // Reset the progress bar to 0 and then force a full render update
          // to show the new snooze state and ensure the progress bar is cleared.
          displayManager.setDismissProgress(0.0f);
          displayManager.update();
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
          Alarm alarm = config.getAlarmById(alarmId);
          if (alarm.getId() != 255)
          {
            alarm.snooze(config.getSnoozeDuration());
            config.setAlarmById(alarmId, alarm);
            config.save();
          }
          alarmManager.stop();

          // Reset the progress bar and then force a full render update
          // to show the new snooze state and ensure the progress bar is cleared.
          displayManager.setDismissProgress(0.0f);
          displayManager.update();
        }
      }
      // Always reset the button timer and action flag on release.
      s_alarmButtonPressTime = 0;
      s_actionTaken = false;
    }
    break;

  case SNOOZED:
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
        displayManager.setDismissProgress(progress);
        displayManager.update();
      }

      // If the button is held long enough, end the snooze for all snoozed alarms
      if (!s_actionTaken && currentMillis - s_alarmButtonPressTime > SNOOZE_DISMISS_HOLD_TIME)
      { // 3-second hold
        SerialLog::getInstance().print("Snooze active: Button held. Ending snooze.\n");
        // Use a fresh copy here since we need to mutate and save
        std::vector<Alarm> snoozedAlarms = config.getAllAlarms();
        for (auto &alarm : snoozedAlarms)
        {
          if (alarm.isSnoozed())
          {
            alarm.dismiss(timeManager.getCachedTime());
            config.setAlarmById(alarm.getId(), alarm);
          }
        }
        config.save();

        // Force the alarm sprite to re-render, which will now be empty
        displayManager.update();

        s_actionTaken = true; // Ensure action is only called once
      }
    }
    else
    {
      // Button is not pressed (it's released)
      if (s_alarmButtonPressTime > 0)
      {
        // If the button was being held, reset the progress bar and timer.
        displayManager.setDismissProgress(0.0f);
        displayManager.update();
        s_alarmButtonPressTime = 0;
      }
    }
    break;

  case IDLE:
    if (snoozeButton.newPressAvailable())
    {
      // --- Interrupt-based logic for normal operations (page cycling) ---
      unsigned long duration = snoozeButton.getPressDuration();
      snoozeButton.clearNewPress();

      SerialLog::getInstance().printf("Button press detected. Duration: %lu ms\n", duration);
      // A short press cycles pages.
      displayManager.cyclePage();
    }
    break;
  }

  // --- Update Alarm Icon (reuses cached alarms vector) ---
  bool anyAlarmEnabled = false;
  bool anyAlarmSnoozed = false;
  for (const auto &alarm : alarms)
  {
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

  handleBootButton();
}
