// ClockWebServer.cpp

#include "ClockWebServer.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "web_content.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include "display.h"
#include "UpdateManager.h"
#include "version.h"

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
    server.onNotFound([this](AsyncWebServerRequest *request)
                      { onCaptivePortalRequest(request); });
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
              Alarm alarm;
              alarm.setId(id);
              alarm.setEnabled(alarmObj["enabled"] | false);
              alarm.setHour(alarmObj["hour"] | 6);
              alarm.setMinute(alarmObj["minute"] | 0);
              alarm.setDays(alarmObj["days"] | 0);
              config.setAlarm(id, alarm);
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
              config.setAutoBrightness(doc["autoBrightness"]);
              config.setBrightness(doc["brightness"]);
              config.set24HourFormat(doc["use24HourFormat"]);
              config.setCelsius(doc["useCelsius"]);
              config.save();
              request->send(200, "text/plain", "Settings saved!");
            }
            delete buffer;
            request->_tempObject = nullptr;
          }
        });

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      request->send(200, "text/plain", "Rebooting...");
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
  }

  // This route is shared between normal and captive portal modes.
  server.on("/wifi/save", HTTP_POST, [this](AsyncWebServerRequest *request)
            { onWifiSaveRequest(request); });

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

void ClockWebServer::onAlarmsRequest(AsyncWebServerRequest *request)
{
  request->send_P(200, "text/html", ALARMS_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onWifiSaveRequest(AsyncWebServerRequest *request)
{
  String ssid = request->arg("ssid");
  String password = request->arg("password");
  auto &config = ConfigManager::getInstance();
  config.setWifiSSID(ssid);
  config.setWifiPassword(password);
  config.save();
  request->send(200, "text/plain", "WiFi credentials saved. Rebooting...");
  delay(1000);
  ESP.restart();
}

void ClockWebServer::onCaptivePortalRequest(AsyncWebServerRequest *request)
{
  // Captive portal detection logic for modern OSes.
  // This handles the OS's connectivity check and keeps the portal open.
  if (!request->host().equals(WiFi.softAPIP().toString()) && !request->url().equals("/"))
  {
    String redirectUrl = "http://" + WiFi.softAPIP().toString() + "/wifi";
    request->redirect(redirectUrl);
    return;
  }

  request->send_P(200, "text/html", WIFI_CONFIG_HTML, [this](const String &var)
                  { return processor(var); });
}

// --- Template Processor ---

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

  return String();
}