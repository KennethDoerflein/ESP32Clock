// ClockWebServer.cpp

#include "ClockWebServer.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "pages.h"
#include "ConfigManager.h"
#include "WiFiManager.h"

// --- GitHub OTA Configuration ---
#define GITHUB_REPO "KennethDoerflein/ESP32_Clock"
#define FIRMWARE_ASSET_NAME "firmware.bin"

// --- File-Scoped Variables and Forward Declarations ---
/// @brief Flag to prevent concurrent OTA updates.
static bool isUpdating = false;

// Forward-declare the handler functions.
static String processor(const String &var);
static void onRootRequest(AsyncWebServerRequest *request);
static void onWifiRequest(AsyncWebServerRequest *request);
static void onWifiSaveRequest(AsyncWebServerRequest *request);
static void onUpdateRequest(AsyncWebServerRequest *request);
static void onUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
static void onGithubUpdate(AsyncWebServerRequest *request);
static void onSettingsRequest(AsyncWebServerRequest *request);
static void onCaptivePortalRequest(AsyncWebServerRequest *request);

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

/**
 * @brief Initializes and starts the asynchronous web server.
 *
 * This method sets up all the URL routes (endpoints) for the web interface
 * and starts the server. The behavior changes if captive portal mode is active.
 */
void ClockWebServer::begin()
{
  // Always handle saving WiFi credentials.
  server.on("/wifi/save", HTTP_POST, onWifiSaveRequest);

  if (_captivePortalActive)
  {
    // In captive portal mode, all other requests serve the setup page.
    server.onNotFound(onCaptivePortalRequest);
  }
  else
  {
    // In normal mode, register all standard application routes.
    server.on("/", HTTP_GET, onRootRequest);
    server.on("/wifi", HTTP_GET, onWifiRequest);
    server.on("/update", HTTP_GET, onUpdateRequest);
    server.on("/update/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, onUpdateUpload);
    server.on("/update/github", HTTP_GET, onGithubUpdate);
    server.on("/settings", HTTP_GET, onSettingsRequest);

    // Settings save handler can remain a lambda as it doesn't need templating.
    server.on("/settings/save", HTTP_POST, [](AsyncWebServerRequest *request)
              {
          if (isUpdating) return;
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
          delay(100); // Give the server time to send the response
          ESP.restart(); });
  }

  server.begin();
}

// --- Handler Implementations ---
/**
 * @brief Template processor for HTML pages.
 * Replaces placeholders like %PLACEHOLDER% with actual content.
 * @param var The placeholder name (e.g., "HEAD", "NETWORKS").
 * @return The replacement string or an empty string if not found.
 */
static String processor(const String &var)
{
  if (var == "HEAD")
  {
    return BOOTSTRAP_HEAD;
  }
  if (var == "NETWORKS")
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

  // Captive Portal specific placeholders
  bool isCaptive = WiFiManager::getInstance().isCaptivePortal();
  if (var == "WIFI_PAGE_TITLE")
  {
    return isCaptive ? "WiFi Setup" : "Configure WiFi";
  }
  if (var == "BACK_BUTTON_CLASS")
  {
    return isCaptive ? "d-none" : "";
  }

  auto &config = ConfigManager::getInstance();
  if (var == "AUTO_BRIGHTNESS_CHECKED")
  {
    return config.isAutoBrightness() ? "checked" : "";
  }
  if (var == "BRIGHTNESS_VALUE")
  {
    return String(config.getBrightness());
  }
  if (var == "IS_24_HOUR_CHECKED")
  {
    return config.is24HourFormat() ? "checked" : "";
  }
  if (var == "USE_CELSIUS_CHECKED")
  {
    return config.isCelsius() ? "checked" : "";
  }
  return String();
}

/**
 * @brief Handles requests to the root URL ("/").
 * Serves the main index page using the template processor.
 */
static void onRootRequest(AsyncWebServerRequest *request)
{
  if (isUpdating)
    return;
  request->send_P(200, "text/html", INDEX_HTML, processor);
}

/**
 * @brief Handles requests to "/wifi".
 * Serves the WiFi configuration page using the template processor.
 */
static void onWifiRequest(AsyncWebServerRequest *request)
{
  if (isUpdating)
    return;
  request->send_P(200, "text/html", WIFI_CONFIG_HTML, processor);
}

/**
 * @brief Handles requests to "/settings".
 * Serves the settings page using the template processor.
 */
static void onSettingsRequest(AsyncWebServerRequest *request)
{
  if (isUpdating)
    return;
  request->send_P(200, "text/html", SETTINGS_PAGE_HTML, processor);
}

/**
 * @brief Handles POST requests to "/wifi/save".
 * Saves the submitted WiFi credentials and restarts the device.
 */
static void onWifiSaveRequest(AsyncWebServerRequest *request)
{
  if (isUpdating)
    return;
  String ssid = request->arg("ssid");
  String password = request->arg("password");
  auto &config = ConfigManager::getInstance();
  config.setWifiSSID(ssid);
  config.setWifiPassword(password);
  config.save();
  request->send(200, "text/plain", "WiFi credentials saved. Restarting to apply...");
  delay(1000);
  ESP.restart();
}

/**
 * @brief Handles requests to "/update".
 * Serves the firmware update page.
 */
static void onUpdateRequest(AsyncWebServerRequest *request)
{
  if (isUpdating)
    return;
  request->send_P(200, "text/html", UPDATE_PAGE_HTML, processor);
}

/**
 * @brief Handles file uploads for firmware updates at "/update/upload".
 * This function processes the firmware binary in chunks.
 */
static void onUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    isUpdating = true;
    Serial.printf("Update started: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
    }
  }
  if (!Update.hasError())
  {
    if (Update.write(data, len) != len)
    {
      Update.printError(Serial);
    }
  }
  if (final)
  {
    if (Update.end(true))
    {
      Serial.println("Update success!");
      request->send(200, "text/plain", "Update successful! Rebooting...");
      delay(1000);
      ESP.restart();
    }
    else
    {
      Update.printError(Serial);
      request->send(500, "text/plain", "Update failed.");
      isUpdating = false;
    }
  }
}

/**
 * @brief Handles requests to "/update/github".
 * Fetches the latest firmware from GitHub and performs an OTA update.
 */
static void onGithubUpdate(AsyncWebServerRequest *request)
{
  if (isUpdating)
  {
    request->send(503, "text/plain", "Update already in progress.");
    return;
  }
  isUpdating = true;
  HTTPClient http;
  WiFiClient client;
  String url = "https://api.github.com/repos/" GITHUB_REPO "/releases/latest";
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    request->send(500, "text/plain", "Failed to fetch release info from GitHub.");
    isUpdating = false;
    return;
  }
  JsonDocument doc;
  deserializeJson(doc, http.getStream());
  http.end();
  String firmwareUrl;
  for (JsonVariant asset : doc["assets"].as<JsonArray>())
  {
    if (asset["name"].as<String>() == FIRMWARE_ASSET_NAME)
    {
      firmwareUrl = asset["browser_download_url"].as<String>();
      break;
    }
  }
  if (firmwareUrl.isEmpty())
  {
    request->send(404, "text/plain", "Firmware asset not found in the latest release.");
    isUpdating = false;
    return;
  }
  request->send(200, "text/plain", "Found update. Starting download...");
  http.begin(client, firmwareUrl);
  int firmwareHttpCode = http.GET();
  if (firmwareHttpCode == HTTP_CODE_OK)
  {
    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
      request->send(500, "text/plain", "GitHub file is empty.");
      isUpdating = false;
      http.end();
      return;
    }
    if (!Update.begin(contentLength))
    {
      Update.printError(Serial);
      request->send(500, "text/plain", "Not enough space to begin OTA");
      isUpdating = false;
      http.end();
      return;
    }
    WiFiClient &stream = http.getStream();
    size_t written = Update.writeStream(stream);
    if (written != contentLength)
    {
      Update.printError(Serial);
      request->send(500, "text/plain", "OTA write failed.");
      isUpdating = false;
      http.end();
      return;
    }
    if (Update.end(true))
    {
      Serial.println("GitHub OTA successful! Rebooting...");
      request->send(200, "text/plain", "Update successful! Rebooting...");
      delay(1000);
      ESP.restart();
    }
    else
    {
      Update.printError(Serial);
      request->send(500, "text/plain", "Update failed to commit.");
      isUpdating = false;
    }
  }
  else
  {
    request->send(firmwareHttpCode, "text/plain", "Failed to download firmware.");
    isUpdating = false;
  }
  http.end();
}

/**
 * @brief Handles all requests when in captive portal mode.
 * Serves the WiFi configuration page, which is dynamically adapted by the processor.
 * @param request The incoming web request.
 */
static void onCaptivePortalRequest(AsyncWebServerRequest *request)
{
  if (isUpdating)
    return;
  request->send_P(200, "text/html", WIFI_CONFIG_HTML, processor);
}