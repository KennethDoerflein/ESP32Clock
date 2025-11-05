/**
 * @file ClockWebServer.cpp
 * @brief Implements the ClockWebServer class for managing the web interface.
 *
 * This file contains the implementation of the web server, including all route
 * handlers, API endpoints, and the template processor for dynamic HTML content.
 * It handles both the initial captive portal setup and the main configuration
 * interface.
 */

#include "ClockWebServer.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "WiFiManager.h"
#include "WebContent.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include "SensorModule.h"
#include "Display.h"
#include "UpdateManager.h"
#include "SerialLog.h"
#include "NtpSync.h"
#include "AlarmManager.h"
#include "Constants.h"

#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

/**
 * @brief Gets the singleton instance of the ClockWebServer.
 * @return A reference to the singleton instance.
 */
ClockWebServer &ClockWebServer::getInstance()
{
  static ClockWebServer instance;
  return instance;
}

/**
 * @brief Constructs a new ClockWebServer.
 * Initializes the web server on port 80.
 */
ClockWebServer::ClockWebServer() : server(80), _captivePortalActive(false) {}

/**
 * @brief Enables captive portal mode.
 *
 * Sets a flag that modifies the server's behavior on `begin()`. This should
 * be called before `begin()` if a captive portal is needed for initial setup.
 */
void ClockWebServer::enableCaptivePortal()
{
  _captivePortalActive = true;
}

/**
 * @brief Starts the web server and sets up all the routes.
 *
 * This is the main setup method for the web server. It checks if captive
 * portal mode is active and registers the appropriate set of URL handlers.
 * It also sets up API endpoints for interacting with the clock's configuration
 * and system functions.
 */
void ClockWebServer::begin()
{
  // Bind class methods to web server routes
  if (_captivePortalActive)
  {
    // --- Captive Portal Handlers ---

    // First, we handle the specific URLs that operating systems use for their
    // connectivity checks. Responding correctly to these prevents the OS from
    // disconnecting or opening its own browser window.
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", "Microsoft Connect Test"); });
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(204); });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", "<!DOCTYPE html><HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });

    // Next, we define the actual setup page that the user should see.
    // This is served from the root URL of the ESP32's IP address.
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
              { onCaptivePortalRequest(request); });

    // Finally, we use the onNotFound handler as a catch-all. If the user's browser
    // requests any other page (e.g., google.com, msn.com), we don't serve content.
    // Instead, we issue an HTTP 302 Redirect, which tells the browser to go to
    // our root setup page. This is the key to reliably forcing the setup page to appear.
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
      // Create the full URL for redirection (e.g., "http://192.168.4.1")
      String redirectUrl = "http://" + request->host();
      request->redirect(redirectUrl); });
  }
  else
  {
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
              { onRootRequest(request); });
    server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
              { onWifiRequest(request); });
    server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest *request)
              { onSettingsRequest(request); });

    server.on("/alarms", HTTP_GET, [this](AsyncWebServerRequest *request)
              { onAlarmsRequest(request); });

    // --- API Handlers for Alarms ---
    server.on("/api/alarms", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      auto& config = ConfigManager::getInstance();
      JsonDocument doc;
      JsonArray alarmsArray = doc.to<JsonArray>();

      for (int i = 0; i < config.getNumAlarms(); ++i) {
        const Alarm& alarm = config.getAlarm(i);
        JsonObject alarmObj = alarmsArray.add<JsonObject>();
        alarmObj["id"] = alarm.getId();
        alarmObj["enabled"] = alarm.isEnabled();
        alarmObj["hour"] = alarm.getHour();
        alarmObj["minute"] = alarm.getMinute();
        alarmObj["days"] = alarm.getDays();
      }
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

    server.on("/api/alarms/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
        
        if (index == 0) {
          // A new request has started. Create a buffer to hold the JSON data.
          // We allocate it on the heap because it can be large.
          request->_tempObject = new std::vector<uint8_t>();
        }

        std::vector<uint8_t>* buffer = (std::vector<uint8_t>*)request->_tempObject;
        buffer->insert(buffer->end(), data, data + len);

        if (index + len == total) {
          // All data has been received.
          JsonDocument doc;
          DeserializationError error = deserializeJson(doc, buffer->data(), buffer->size());
          delete buffer; // Clean up the buffer
          request->_tempObject = nullptr;

          if (error) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
          }

          JsonArray alarmsArray = doc.as<JsonArray>();
          if (alarmsArray.isNull()) {
            request->send(400, "text/plain", "Expected a JSON array");
            return;
          }

          auto& config = ConfigManager::getInstance();
          for (JsonObject alarmObj : alarmsArray) {
            int id = alarmObj["id"] | -1;
            if (id >= 0 && id < config.getNumAlarms()) {
              // Get the existing alarm to preserve its snooze state
              Alarm alarm = config.getAlarm(id);
              bool isEnabled = alarmObj["enabled"] | false;
              
              // If the alarm is being disabled and it's the one currently ringing, stop it.
              if (!isEnabled && alarm.isEnabled() && AlarmManager::getInstance().isRinging() && AlarmManager::getInstance().getActiveAlarmId() == id) {
                AlarmManager::getInstance().stop();
              }

              alarm.setEnabled(isEnabled);
              alarm.setHour(alarmObj["hour"] | 6);
              alarm.setMinute(alarmObj["minute"] | 0);
              alarm.setDays(alarmObj["days"] | 0);

              config.setAlarm(id, alarm);
            }
          }
          request->send(200, "text/plain", "Alarms saved successfully!");
        } });

    // --- API Handlers for Settings ---
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      auto& config = ConfigManager::getInstance();
      JsonDocument doc;
      doc["autoBrightness"] = config.isAutoBrightness();
      doc["brightness"] = config.getBrightness();
      doc["autoBrightnessStartHour"] = config.getAutoBrightnessStartHour();
      doc["autoBrightnessEndHour"] = config.getAutoBrightnessEndHour();
      doc["dayBrightness"] = config.getDayBrightness();
      doc["nightBrightness"] = config.getNightBrightness();
      doc["actualBrightness"] = Display::getInstance().getActualBrightness();
      doc["use24HourFormat"] = config.is24HourFormat();
      doc["useCelsius"] = config.isCelsius();
      doc["screenFlipped"] = config.isScreenFlipped();
      doc["invertColors"] = config.isInvertColors();
      doc["timezone"] = config.getTimezone();
      doc["snoozeDuration"] = config.getSnoozeDuration();
      doc["dismissDuration"] = config.getDismissDuration();
      doc["tempCorrectionEnabled"] = config.isTempCorrectionEnabled();
      doc["tempCorrection"] = config.getTempCorrection();
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

    // API handler for settings save
    server.on(
        "/api/settings/save", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
           size_t index, size_t total)
        {
          if (index == 0)
          {
            request->_tempObject = new std::vector<uint8_t>();
          }

          std::vector<uint8_t> *buffer =
              (std::vector<uint8_t> *)request->_tempObject;
          buffer->insert(buffer->end(), data, data + len);

          if (index + len == total)
          {
            JsonDocument doc;
            if (deserializeJson(doc, buffer->data(), buffer->size()))
            {
              request->send(400, "text/plain", "Invalid JSON");
            }
            else
            {
              auto &config = ConfigManager::getInstance();
              String oldTimezone = config.getTimezone();
              bool oldScreenFlipped = config.isScreenFlipped();
              bool oldInvertColors = config.isInvertColors();
              float oldTempCorrection = config.getTempCorrection();
              bool oldTempCorrectionEnabled = config.isTempCorrectionEnabled();
              config.setAutoBrightness(doc["autoBrightness"]);
              config.setBrightness(doc["brightness"]);
              config.setAutoBrightnessStartHour(doc["autoBrightnessStartHour"]);
              config.setAutoBrightnessEndHour(doc["autoBrightnessEndHour"]);
              config.setDayBrightness(doc["dayBrightness"]);
              config.setNightBrightness(doc["nightBrightness"]);
              config.set24HourFormat(doc["use24HourFormat"]);
              config.setCelsius(doc["useCelsius"]);
              config.setScreenFlipped(doc["screenFlipped"]);
              config.setInvertColors(doc["invertColors"]);
              config.setTimezone(doc["timezone"]);
              config.setSnoozeDuration(doc["snoozeDuration"]);
              config.setDismissDuration(doc["dismissDuration"]);
              config.setTempCorrectionEnabled(doc["tempCorrectionEnabled"]);
              config.setTempCorrection(doc["tempCorrection"]);

              if (oldScreenFlipped != config.isScreenFlipped())
              {
                Display::getInstance().updateRotation();
                DisplayManager::getInstance().requestFullRefresh();
              }

              if (oldInvertColors != config.isInvertColors())
              {
                Display::getInstance().updateInversion();
                DisplayManager::getInstance().requestFullRefresh();
              }

              if (oldTimezone != config.getTimezone())
              {
                startNtpSync();
              }

              if (oldTempCorrection != config.getTempCorrection() || oldTempCorrectionEnabled != config.isTempCorrectionEnabled())
              {
                handleSensorUpdates(true);
              }

              request->send(200, "text/plain", "Settings saved!");
            }
            delete buffer;
            request->_tempObject = nullptr;
          }
        });

    server.on("/api/settings/reset", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      auto &config = ConfigManager::getInstance();
      bool oldScreenFlipped = config.isScreenFlipped();
      config.resetGeneralSettingsToDefaults();
      if (oldScreenFlipped != config.isScreenFlipped())
      {
        Display::getInstance().updateRotation();
      }
      DisplayManager::getInstance().requestFullRefresh();
      
      // Give the main loop time to process the new default settings
      // and update the Display singleton's actualBrightness.
      delay(100); 

      request->send(200, "text/plain", "General settings reset!"); });

    // --- API Handlers for Display ---
    server.on("/api/display", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      auto& config = ConfigManager::getInstance();
      JsonDocument doc;
      doc["backgroundColor"] = config.getBackgroundColor();
      doc["timeColor"] = config.getTimeColor();
      doc["todColor"] = config.getTodColor();
      doc["secondsColor"] = config.getSecondsColor();
      doc["dayOfWeekColor"] = config.getDayOfWeekColor();
      doc["dateColor"] = config.getDateColor();
      doc["tempColor"] = config.getTempColor();
      doc["humidityColor"] = config.getHumidityColor();
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

    server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      JsonDocument doc;
      doc["bmeFound"] = isBmeFound();
      if (isBmeFound()) {
        doc["bmeTemp"] = String(getBmeTemperature(), 1);
        doc["bmeHumidity"] = String(getHumidity(), 1);
      }
      if (isRtcFound()) {
        doc["rtcTemp"] = String(getRtcTemperature(), 1);
      }
      doc["unit"] = ConfigManager::getInstance().isCelsius() ? "C" : "F";
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

    server.on("/api/system/stats", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      JsonDocument doc;
      doc["freeHeap"] = ESP.getFreeHeap();
      doc["uptime"] = millis();
      doc["rssi"] = WiFi.RSSI();
      doc["coreTemp"] = String(getCoreTemperature(), 1);
      doc["unit"] = ConfigManager::getInstance().isCelsius() ? "C" : "F";
      
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

    server.on("/api/system/ntp-sync", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      startNtpSync();
      request->send(200, "text/plain", "NTP sync started successfully."); });

    server.on(
        "/api/display/save", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len,
           size_t index, size_t total)
        {
          if (index == 0)
          {
            request->_tempObject = new std::vector<uint8_t>();
          }

          std::vector<uint8_t> *buffer =
              (std::vector<uint8_t> *)request->_tempObject;
          buffer->insert(buffer->end(), data, data + len);

          if (index + len == total)
          {
            JsonDocument doc;
            if (deserializeJson(doc, buffer->data(), buffer->size()))
            {
              request->send(400, "text/plain", "Invalid JSON");
            }
            else
            {
              auto &config = ConfigManager::getInstance();
              String oldBgColor = config.getBackgroundColor();
              String newBgColor = doc["backgroundColor"].as<String>();

              config.setBackgroundColor(newBgColor);
              config.setTimeColor(doc["timeColor"].as<String>());
              config.setTodColor(doc["todColor"].as<String>());
              config.setSecondsColor(doc["secondsColor"].as<String>());
              config.setDayOfWeekColor(doc["dayOfWeekColor"].as<String>());
              config.setDateColor(doc["dateColor"].as<String>());
              config.setTempColor(doc["tempColor"].as<String>());
              config.setHumidityColor(doc["humidityColor"].as<String>());

              if (oldBgColor != newBgColor)
              {
                DisplayManager::getInstance().requestFullRefresh();
              }
              else
              {
                DisplayManager::getInstance().requestPartialRefresh();
              }

              request->send(200, "text/plain", "Display settings saved!");
            }
            delete buffer;
            request->_tempObject = nullptr;
          }
        });

    server.on("/api/display/reset", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      auto &config = ConfigManager::getInstance();
      config.resetDisplayToDefaults();
      DisplayManager::getInstance().requestFullRefresh();
      
      // Give the main loop time to process the new default settings.
      delay(100); 

      request->send(200, "text/plain", "Display settings reset!"); });

    server.on("/api/wifi/hostname", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("hostname", true)) {
            String hostname = request->getParam("hostname", true)->value();
            WiFiManager::getInstance().setHostname(hostname);
            request->send(200, "text/plain", "Hostname saved. Rebooting...");
            delay(100);
            ESP.restart();
        } else {
            request->send(400, "text/plain", "Hostname not provided.");
        } });

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      request->send(200, "text/plain", "Rebooting...");
      delay(100);
      ESP.restart(); });

    server.on("/factory-reset", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      if (UpdateManager::getInstance().isUpdateInProgress())
      {
        request->send(409, "text/plain", "Update in progress. Cannot perform factory reset.");
        return;
      }
      request->send(200, "text/plain", "Performing factory reset...");
      ConfigManager::getInstance().factoryReset();
      delay(100);
      ESP.restart(); });

    server.on("/factory-reset-except-wifi", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      if (UpdateManager::getInstance().isUpdateInProgress())
      {
        request->send(409, "text/plain", "Update in progress. Cannot perform factory reset.");
        return;
      }
      request->send(200, "text/plain", "Performing factory reset and keeping WiFi credentials...");
      ConfigManager::getInstance().factoryResetExceptWiFi();
      delay(100);
      ESP.restart(); });

    server.on("/system", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", SYSTEM_PAGE_HTML, [this](const String &var)
                                { return processor(var); }); });

    server.on(
        "/update", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
          // This handler is intentionally empty. The response is sent in the upload handler.
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
              request->send(200, "text/plain", "Update successful! Rebooting...");
              delay(1000); // Give client time to receive response
              ESP.restart();
            }
            else
            {
              request->send(500, "text/plain", "Update failed. Check serial monitor for details.");
            }
          }
        });

    server.on("/api/update/github", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      String status = UpdateManager::getInstance().handleGithubUpdate();
      request->send(200, "text/plain", status); });

    server.on("/api/update/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      bool inProgress = UpdateManager::getInstance().isUpdateInProgress();
      request->send(200, "application/json", String("{\"inProgress\":") + (inProgress ? "true" : "false") + "}"); });

    if (String(FIRMWARE_VERSION).indexOf("dev") != -1)
    {
      SerialLog::getInstance().begin(&server);
    }
  }

  // This route is shared between normal and captive portal modes.
  server.on("/wifi/save", HTTP_POST, [this](AsyncWebServerRequest *request)
            { onWifiSaveRequest(request); });

  server.on("/wifi/test", HTTP_POST, [this](AsyncWebServerRequest *request)
            { onWifiTestRequest(request); });

  server.on("/wifi/status", HTTP_GET, [this](AsyncWebServerRequest *request)
            { onWifiStatusRequest(request); });

  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    auto &wifiManager = WiFiManager::getInstance();
    if (request->hasParam("start") && request->getParam("start")->value() == "true")
    {
      wifiManager.startScan();
      // Immediately return a scanning status
      request->send(200, "application/json", "{\"status\":\"scanning\"}");
    }
    else
    {
      String json = wifiManager.getScanResults();
      request->send(200, "application/json", json);
    } });

  server.begin();
}

/**
 * @brief Handles requests to the root URL ("/").
 * Serves the main index page.
 * @param request The incoming web request.
 */
void ClockWebServer::onRootRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", INDEX_HTML, [this](const String &var)
                  { return processor(var); });
}

/**
 * @brief Handles requests for the WiFi configuration page.
 * @param request The incoming web request.
 */
void ClockWebServer::onWifiRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", WIFI_CONFIG_HTML, [this](const String &var)
                  { return processor(var); });
}

/**
 * @brief Handles requests for the settings page.
 * @param request The incoming web request.
 */
void ClockWebServer::onSettingsRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", SETTINGS_PAGE_HTML, [this](const String &var)
                  { return settingsProcessor(var); });
}

/**
 * @brief Handles requests for the alarms configuration page.
 * @param request The incoming web request.
 */
void ClockWebServer::onAlarmsRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", ALARMS_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

/**
 * @brief Handles the form submission for saving WiFi credentials.
 *
 * This function's behavior depends on the mode. In captive portal mode, it
 * starts a non-blocking connection test. In normal mode, it saves the
 * credentials and reboots the device.
 *
 * @param request The incoming web request.
 */
void ClockWebServer::onWifiSaveRequest(AsyncWebServerRequest *request)
{
  String ssid = request->arg("ssid");
  if (ssid.length() == 0)
  {
    request->send(400, "text/plain", "SSID cannot be empty.");
    return;
  }
  String password = request->arg("password");

  auto &wifiManager = WiFiManager::getInstance();
  if (wifiManager.isCaptivePortal())
  {
    // In captive portal, start a non-blocking test and let the client poll for status.
    wifiManager.startConnectionTest(ssid, password, true);
    request->send(200, "text/plain", "Test started. Polling for status...");
  }
  else
  {
    // If not in captive portal, just save, reboot, and apply the new settings.
    request->send(200, "text/plain", "Credentials saved. Rebooting to connect...");
    // Add a small delay to ensure the response is sent before rebooting.
    delay(500);
    wifiManager.saveCredentialsAndReboot(ssid, password);
  }
}

/**
 * @brief Handles a request to test WiFi credentials without saving them.
 * @param request The incoming web request.
 */
void ClockWebServer::onWifiTestRequest(AsyncWebServerRequest *request)
{
  String ssid = request->arg("ssid");
  if (ssid.length() == 0)
  {
    request->send(400, "text/plain", "SSID cannot be empty.");
    return;
  }
  String password = request->arg("password");

  // "Test" only uses polling, it never saves the credentials.
  WiFiManager::getInstance().startConnectionTest(ssid, password, false);
  request->send(200, "text/plain", "Test started. Polling for status...");
}

/**
 * @brief Handles API requests for the status of a WiFi connection test.
 *
 * This is used by the client to poll for the result of a connection test
 * initiated by `onWifiSaveRequest` or `onWifiTestRequest`.
 *
 * @param request The incoming web request.
 */
void ClockWebServer::onWifiStatusRequest(AsyncWebServerRequest *request)
{
  auto &wifiManager = WiFiManager::getInstance();
  auto status = wifiManager.getConnectionTestStatus();
  switch (status)
  {
  case WiFiManager::TEST_IN_PROGRESS:
    request->send(202, "text/plain", "testing");
    break;
  case WiFiManager::TEST_SUCCESS:
    request->send(200, "text/plain", "success");
    // Check if a reboot is pending *after* sending the success response.
    if (wifiManager.isPendingReboot())
    {
      delay(500); // Give the client a moment to receive the response
      ESP.restart();
    }
    wifiManager.resetConnectionTestStatus();
    break;
  case WiFiManager::TEST_FAILED:
    request->send(401, "text/plain", "failed");
    wifiManager.resetConnectionTestStatus();
    break;
  case WiFiManager::TEST_IDLE:
  default:
    request->send(200, "text/plain", "idle");
    break;
  }
}

/**
 * @brief Serves the simplified WiFi setup page for the captive portal.
 * @param request The incoming web request.
 */
void ClockWebServer::onCaptivePortalRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", SIMPLE_WIFI_SETUP_HTML);
}

/**
 * @brief Handles captive portal redirection for various operating systems.
 * @param request The incoming web request.
 */
void ClockWebServer::onCaptivePortalRedirect(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse(302);
  response->addHeader("Location", "http://" + WiFi.softAPIP().toString());
  request->send(response);
}

/**
 * @brief Converts a raw brightness value (0-255) to a percentage string.
 * @param brightness The raw brightness value.
 * @return A string representing the brightness as a percentage (e.g., "50%").
 */
String brightnessToPercent(int brightness)
{
  // Clamp the value to the allowed range
  brightness = max(BRIGHTNESS_MIN, min(BRIGHTNESS_MAX, brightness));
  // Map the value to a percentage
  int percentage = map(brightness, BRIGHTNESS_MIN, BRIGHTNESS_MAX, 0, 100);
  return String(percentage) + "&#37;";
}

/**
 * @brief Formats an hour value for display in the web UI.
 * @param hour The hour to format (0-23).
 * @param is24Hour True if the display should be in 24-hour format.
 * @return A formatted string (e.g., "07", "12 PM").
 */
String formatHour(int hour, bool is24Hour)
{
  if (is24Hour)
  {
    if (hour < 10)
    {
      return "0" + String(hour);
    }
    return String(hour);
  }
  else
  {
    if (hour == 0)
      return "12 AM";
    if (hour == 12)
      return "12 PM";
    if (hour < 12)
      return String(hour) + " AM";
    return String(hour - 12) + " PM";
  }
}

/**
 * @brief Sets up the mDNS responder.
 *
 * This allows the device to be accessed on the local network using a
 * hostname like "ESP32Clock_XXXXXX.local".
 */
void ClockWebServer::setupMDNS()
{
  // Start the mDNS responder for ESP32Clock_XXXXXX.local
  String hostname = WiFiManager::getInstance().getHostname();
  if (MDNS.begin(hostname.c_str()))
  {
    SerialLog::getInstance().print("mDNS responder started\n");
    MDNS.addService("http", "tcp", 80); // Advertise the web server
  }
  else
  {
    SerialLog::getInstance().print("Error starting mDNS!\n");
  }
}

/**
 * @brief Processes placeholders in the HTML templates.
 *
 * This function is used as a callback by the web server to replace
 * placeholders (like `%%FIRMWARE_VERSION%%`) in the HTML files with
 * dynamic values.
 *
 * @param var The name of the placeholder variable.
 * @return The value to be substituted for the placeholder.
 */
String ClockWebServer::processor(const String &var)
{
  if (var == "HEAD")
    return BOOTSTRAP_HEAD;
  if (var == "WIFI_PAGE_TITLE")
    return WiFiManager::getInstance().isCaptivePortal() ? "WiFi Setup" : "Configure WiFi";
  if (var == "BACK_BUTTON_CLASS")
    return WiFiManager::getInstance().isCaptivePortal() ? "d-none" : "";
  if (var == "FIRMWARE_VERSION")
    return FIRMWARE_VERSION;
  if (var == "IP_ADDRESS")
    return WiFi.localIP().toString();
  if (var == "HOSTNAME")
    return WiFiManager::getInstance().getHostname();
  if (var == "SERIAL_LOG_TAB")
  {
    if (String(FIRMWARE_VERSION).indexOf("dev") != -1)
    {
      return SERIAL_LOG_TAB_HTML;
    }
    return "";
  }
  if (var == "SERIAL_LOG_TAB_PANE")
  {
    if (String(FIRMWARE_VERSION).indexOf("dev") != -1)
    {
      return SERIAL_LOG_TAB_PANE_HTML;
    }
    return "";
  }
  if (var == "SERIAL_LOG_SCRIPT")
  {
    if (String(FIRMWARE_VERSION).indexOf("dev") != -1)
    {
      return SERIAL_LOG_SCRIPT_JS;
    }
    return "";
  }

  return String();
}

struct TimezoneInfo
{
  const char *posixString;
  const char *placeholder;
};

static const TimezoneInfo timezones[] = {
    {"EST5EDT,M3.2.0/2:00,M11.1.0/2:00", "TIMEZONE_SELECTED_EST"},
    {"CST6CDT,M3.2.0/2:00,M11.1.0/2:00", "TIMEZONE_SELECTED_CST"},
    {"MST7MDT,M3.2.0/2:00,M11.1.0/2:00", "TIMEZONE_SELECTED_MST"},
    {"PST8PDT,M3.2.0/2:00,M11.1.0/2:00", "TIMEZONE_SELECTED_PST"},
    {"MST7", "TIMEZONE_SELECTED_AZ"},
    {"AKST9AKDT,M3.2.0/2:00,M11.1.0/2:00", "TIMEZONE_SELECTED_AK"},
    {"HST10", "TIMEZONE_SELECTED_HI"}};

/**
 * @brief A specialized template processor for the settings page.
 *
 * This extends the base processor with additional placeholders specific to
 * the settings and display configuration pages.
 *
 * @param var The name of the placeholder variable.
 * @return The value to be substituted.
 */
String ClockWebServer::settingsProcessor(const String &var)
{
  auto &config = ConfigManager::getInstance();
  String value = processor(var);
  if (value != String())
  {
    return value;
  }

  if (var == "BRIGHTNESS_MIN")
    return String(BRIGHTNESS_MIN);
  if (var == "BRIGHTNESS_MAX")
    return String(BRIGHTNESS_MAX);
  if (var == "AUTO_BRIGHTNESS_CHECKED")
    return config.isAutoBrightness() ? "checked" : "";
  if (var == "AUTO_BRIGHTNESS_START_HOUR")
    return String(config.getAutoBrightnessStartHour());
  if (var == "AUTO_BRIGHTNESS_START_HOUR_VALUE")
    return String(formatHour(config.getAutoBrightnessStartHour(), config.is24HourFormat()));
  if (var == "AUTO_BRIGHTNESS_END_HOUR")
    return String(config.getAutoBrightnessEndHour());
  if (var == "AUTO_BRIGHTNESS_END_HOUR_VALUE")
    return String(formatHour(config.getAutoBrightnessEndHour(), config.is24HourFormat()));
  if (var == "DAY_BRIGHTNESS")
    return String(config.getDayBrightness());
  if (var == "DAY_BRIGHTNESS_VALUE")
    return String(brightnessToPercent(config.getDayBrightness()));
  if (var == "NIGHT_BRIGHTNESS")
    return String(config.getNightBrightness());
  if (var == "NIGHT_BRIGHTNESS_VALUE")
    return String(brightnessToPercent(config.getNightBrightness()));
  if (var == "BRIGHTNESS")
    return String(Display::getInstance().getActualBrightness());
  if (var == "BRIGHTNESS_VALUE")
    return String(brightnessToPercent(Display::getInstance().getActualBrightness()));
  if (var == "USE_24_HOUR_FORMAT_CHECKED")
    return config.is24HourFormat() ? "checked" : "";
  if (var == "USE_CELSIUS_CHECKED")
    return config.isCelsius() ? "checked" : "";
  if (var == "AUTO_BRIGHTNESS_CONTROLS_CLASS")
    return config.isAutoBrightness() ? "" : "d-none";
  if (var == "MANUAL_BRIGHTNESS_CLASS")
    return config.isAutoBrightness() ? "d-none" : "";
  if (var == "SCREEN_FLIPPED_CHECKED")
    return config.isScreenFlipped() ? "checked" : "";
  if (var == "INVERT_COLORS_CHECKED")
    return config.isInvertColors() ? "checked" : "";
  if (var == "BACKGROUND_COLOR")
    return config.getBackgroundColor();
  if (var == "TIME_COLOR")
    return config.getTimeColor();
  if (var == "TOD_COLOR")
    return config.getTodColor();
  if (var == "SECONDS_COLOR")
    return config.getSecondsColor();
  if (var == "DAY_OF_WEEK_COLOR")
    return config.getDayOfWeekColor();
  if (var == "DATE_COLOR")
    return config.getDateColor();
  if (var == "TEMP_COLOR")
    return config.getTempColor();
  if (var == "HUMIDITY_COLOR")
    return config.getHumidityColor();
  if (var == "SNOOZE_DURATION")
    return String(config.getSnoozeDuration());
  if (var == "DISMISS_DURATION")
    return String(config.getDismissDuration());
  if (var == "TEMP_CORRECTION_VALUE")
  {
    float offset = config.getTempCorrection();
    if (!config.isCelsius())
    {
      offset = offset * 9.0 / 5.0;
    }
    return String(offset, 1);
  }
  if (var == "TEMP_CORRECTION_UNIT")
    return config.isCelsius() ? "C" : "F";
  if (var == "TEMP_CORRECTION_ENABLED_CHECKED")
    return config.isTempCorrectionEnabled() ? "checked" : "";
  if (var == "TEMP_CORRECTION_CONTROLS_CLASS")
    return config.isTempCorrectionEnabled() ? "" : "d-none";

  // Timezone selections
  String timezone = config.getTimezone();
  for (const auto &tz : timezones)
  {
    if (var == tz.placeholder)
    {
      return timezone == tz.posixString ? "selected" : "";
    }
  }
  return String();
}