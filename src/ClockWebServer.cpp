// ClockWebServer.cpp

#include "ClockWebServer.h"
#include "ConfigManager.h"
#include "OtaManager.h"
#include "WiFiManager.h"
#include "web_content.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <WiFi.h>

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

    // Lambda for settings save; it's simple enough not to need a full method.
    server.on("/settings/save", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      if (OtaManager::getInstance().isUpdating()) return;
      auto &config = ConfigManager::getInstance();
      config.setAutoBrightness(request->hasParam("autoBrightness"));
      config.setBrightness(request->arg("brightness").toInt());
      config.set24HourFormat(request->hasParam("use24HourFormat"));
      config.setCelsius(request->hasParam("useCelsius"));
      config.save();
      request->send(200, "text/plain", "Settings saved successfully!"); });

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      request->send(200, "text/plain", "Rebooting...");
      delay(100);
      ESP.restart(); });
  }

  // This route is shared between normal and captive portal modes.
  server.on("/wifi/save", HTTP_POST, [this](AsyncWebServerRequest *request)
            { onWifiSaveRequest(request); });

  // Initialize and register OTA manager routes, passing the processor function.
  OtaManager::getInstance().begin(server, [this](const String &var)
                                  { return processor(var); });

  server.begin();
}

// --- Private Methods ---

// --- Request Handlers ---

void ClockWebServer::onRootRequest(AsyncWebServerRequest *request)
{
  if (OtaManager::getInstance().isUpdating())
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }
  request->send_P(200, "text/html", INDEX_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onWifiRequest(AsyncWebServerRequest *request)
{
  if (OtaManager::getInstance().isUpdating())
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }
  request->send_P(200, "text/html", WIFI_CONFIG_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onSettingsRequest(AsyncWebServerRequest *request)
{
  if (OtaManager::getInstance().isUpdating())
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }
  request->send_P(200, "text/html", SETTINGS_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onAlarmsRequest(AsyncWebServerRequest *request)
{
  if (OtaManager::getInstance().isUpdating())
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }
  request->send_P(200, "text/html", ALARMS_PAGE_HTML, [this](const String &var)
                  { return processor(var); });
}

void ClockWebServer::onWifiSaveRequest(AsyncWebServerRequest *request)
{
  if (OtaManager::getInstance().isUpdating())
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }
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
  if (OtaManager::getInstance().isUpdating())
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }

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
  if (var == "NETWORKS")
    return getNetworksList();
  if (var == "WIFI_PAGE_TITLE")
    return WiFiManager::getInstance().isCaptivePortal() ? "WiFi Setup" : "Configure WiFi";
  if (var == "BACK_BUTTON_CLASS")
    return WiFiManager::getInstance().isCaptivePortal() ? "d-none" : "";

  // Settings placeholders
  auto &config = ConfigManager::getInstance();
  if (var == "AUTO_BRIGHTNESS_CHECKED")
    return config.isAutoBrightness() ? "checked" : "";
  if (var == "BRIGHTNESS_VALUE")
    return String(config.getBrightness());
  if (var == "IS_24_HOUR_CHECKED")
    return config.is24HourFormat() ? "checked" : "";
  if (var == "USE_CELSIUS_CHECKED")
    return config.isCelsius() ? "checked" : "";

  return String();
}

String ClockWebServer::getNetworksList()
{
  String networks_html;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i)
  {
    networks_html += "<a href='#' class='list-group-item list-group-item-action d-flex justify-content-between align-items-center' onclick='event.preventDefault(); selectNetwork(\"" + WiFi.SSID(i) + "\")'>";
    networks_html += WiFi.SSID(i);
    networks_html += "<span class='badge bg-primary rounded-pill'>" + String(WiFi.RSSI(i)) + " dBm</span>";
    networks_html += "</a>";
  }
  return networks_html;
}