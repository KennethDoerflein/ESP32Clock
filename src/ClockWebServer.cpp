// src/ClockWebServer.cpp

#include "ClockWebServer.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "WiFiManager.h"
#include "web_content.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include "display.h"
#include "UpdateManager.h"
#include "version.h"
#include "SerialLog.h"

// --- Singleton Implementation ---
ClockWebServer &ClockWebServer::getInstance()
{
  static ClockWebServer instance;
  return instance;
}

// --- Constructor ---
ClockWebServer::ClockWebServer() : server(80), _captivePortalActive(false) {}

// --- Public Methods ---

/**
 * @brief Enables captive portal mode.
 * Sets a flag that modifies the server's behavior on `begin()`.
 */
void ClockWebServer::enableCaptivePortal()
{
  _captivePortalActive = true;
}

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

    server.on("/display", HTTP_GET, [this](AsyncWebServerRequest *request)
              { onDisplayRequest(request); });

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
              Alarm newAlarm;
              newAlarm.setId(id);
              newAlarm.setEnabled(alarmObj["enabled"] | false);
              newAlarm.setHour(alarmObj["hour"] | 6);
              newAlarm.setMinute(alarmObj["minute"] | 0);
              newAlarm.setDays(alarmObj["days"] | 0);

              // Preserve snooze state
              const Alarm& oldAlarm = config.getAlarm(id);
              if (oldAlarm.isSnoozed()) {
                newAlarm.setSnoozeState(true, oldAlarm.getSnoozeUntil());
              }

              config.setAlarm(id, newAlarm);
            }
          }
          config.save();
          request->send(200, "text/plain", "Alarms saved successfully!");
        } });

    // --- API Handlers for Settings ---
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      auto& config = ConfigManager::getInstance();
      JsonDocument doc;
      doc["autoBrightness"] = config.isAutoBrightness();
      doc["brightness"] = config.getBrightness();
      doc["actualBrightness"] = Display::getInstance().getActualBrightness();
      doc["use24HourFormat"] = config.is24HourFormat();
      doc["useCelsius"] = config.isCelsius();
      doc["screenFlipped"] = config.isScreenFlipped();
      
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
              bool oldScreenFlipped = config.isScreenFlipped();
              config.setAutoBrightness(doc["autoBrightness"]);
              config.setBrightness(doc["brightness"]);
              config.set24HourFormat(doc["use24HourFormat"]);
              config.setCelsius(doc["useCelsius"]);
              config.setScreenFlipped(doc["screenFlipped"]);
              config.save();

              if (oldScreenFlipped != config.isScreenFlipped())
              {
                Display::getInstance().updateRotation();
                DisplayManager::getInstance().requestFullRefresh();
              }

              request->send(200, "text/plain", "Settings saved!");
            }
            delete buffer;
            request->_tempObject = nullptr;
          }
        });

    server.on("/api/display/reset", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      auto &config = ConfigManager::getInstance();
      config.resetDisplayToDefaults();
      config.save();
      DisplayManager::getInstance().requestFullRefresh();
      request->send(200, "text/plain", "Display settings reset!"); });

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
              config.save();

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
      request->send(200, "text/plain", "Performing factory reset...");
      ConfigManager::getInstance().factoryReset();
      delay(100);
      ESP.restart(); });

    server.on("/update", HTTP_GET, [this](AsyncWebServerRequest *request)
              { request->send_P(200, "text/html", UPDATE_PAGE_HTML, [this](const String &var)
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

    if (String(FIRMWARE_VERSION).indexOf("dev") != -1)
    {
      server.on("/seriallog", HTTP_GET, [this](AsyncWebServerRequest *request)
                { request->send_P(200, "text/html", SERIAL_LOG_PAGE_HTML,
                                  [this](const String &var)
                                  { return processor(var); }); });
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
    String json = WiFiManager::getInstance().scanNetworksAsync();
    request->send(200, "application/json", json); });

  server.begin();
}

// --- Private Methods ---

// --- Request Handlers ---

void ClockWebServer::onRootRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", INDEX_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onWifiRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", WIFI_CONFIG_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onSettingsRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", SETTINGS_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onDisplayRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", DISPLAY_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onAlarmsRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", ALARMS_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

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

void ClockWebServer::onCaptivePortalRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", SIMPLE_WIFI_SETUP_HTML);
}

void ClockWebServer::onCaptivePortalRedirect(AsyncWebServerRequest *request)
{
  AsyncWebServerResponse *response = request->beginResponse(302);
  response->addHeader("Location", "http://" + WiFi.softAPIP().toString());
  request->send(response);
}

// --- Template Processor ---

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
  if (var == "SERIAL_LOG_BUTTON")
  {
    if (String(FIRMWARE_VERSION).indexOf("dev") != -1)
    {
      return "<a href=\"/seriallog\" class=\"btn btn-outline-light btn-lg\">Serial Log</a>";
    }
    return "";
  }

  return String();
}