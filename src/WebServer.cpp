// WebServer.cpp

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "WebServer.h"
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "pages.h"
#include "ConfigManager.h"

// --- GitHub OTA Configuration ---
// The repository to check for firmware updates.
#define GITHUB_REPO "KennethDoerflein/ESP32_Clock"
// The name of the firmware binary asset in the GitHub release.
#define FIRMWARE_ASSET_NAME "firmware.bin"

// Use an anonymous namespace to limit the scope of handlers and server instance to this file.
namespace
{
  // Create an instance of the asynchronous web server on port 80.
  AsyncWebServer server(80);
  // Flag to prevent concurrent OTA updates.
  bool isUpdating = false;

  /**
   * @brief Template processor for HTML pages.
   * Replaces placeholders like %HEAD% with actual content.
   * @param var The placeholder name (e.g., "HEAD").
   * @return The replacement string or an empty string if not found.
   */
  String processor(const String &var)
  {
    if (var == "HEAD")
      return BOOTSTRAP_HEAD; // Inject Bootstrap CSS header.
    return String();
  }

  /**
   * @brief Handles requests to the root URL ("/").
   * Serves the main index page.
   */
  void onRootRequest(AsyncWebServerRequest *request)
  {
    if (isUpdating)
      return;
    request->send_P(200, "text/html", INDEX_HTML, processor);
  }

  /**
   * @brief Handles requests to "/wifi".
   * Scans for available WiFi networks and serves the WiFi configuration page.
   */
  void onWifiRequest(AsyncWebServerRequest *request)
  {
    if (isUpdating)
      return;
    String networks_html;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i)
    {
      // Create a Bootstrap list item for each network.
      networks_html += "<a href='#' class='list-group-item list-group-item-action d-flex justify-content-between align-items-center' onclick='event.preventDefault(); selectNetwork(\"" + WiFi.SSID(i) + "\")'>";
      networks_html += WiFi.SSID(i);
      networks_html += "<span class='badge bg-primary rounded-pill'>" + String(WiFi.RSSI(i)) + " dBm</span>";
      networks_html += "</a>";
    }

    String page = WIFI_CONFIG_HTML;
    page.replace("%HEAD%", BOOTSTRAP_HEAD);
    page.replace("%NETWORKS%", networks_html);

    request->send(200, "text/html", page);
  }

  /**
   * @brief Handles POST requests to "/wifi/save".
   * Saves the submitted WiFi credentials and restarts the device.
   */
  void onWifiSaveRequest(AsyncWebServerRequest *request)
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
    delay(1000); // Wait for the response to be sent.
    ESP.restart();
  }

  /**
   * @brief Handles requests to "/update".
   * Serves the firmware update page.
   */
  void onUpdateRequest(AsyncWebServerRequest *request)
  {
    if (isUpdating)
      return;
    request->send_P(200, "text/html", UPDATE_PAGE_HTML, processor);
  }

  /**
   * @brief Handles file uploads for firmware updates at "/update/upload".
   * This function processes the firmware binary in chunks.
   */
  void onUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
  {
    if (!index)
    { // First chunk
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
    { // Last chunk
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
  void onGithubUpdate(AsyncWebServerRequest *request)
  {
    if (isUpdating)
    {
      request->send(503, "text/plain", "Update already in progress.");
      return;
    }
    isUpdating = true;

    HTTPClient http;
    WiFiClient client;

    // Fetch the latest release information from the GitHub API.
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

    // Parse the JSON response to find the firmware asset URL.
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

    // Inform the client that the download is starting.
    request->send(200, "text/plain", "Found update. Starting download...");

    // Perform the OTA update using the firmware URL.
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

      // Write the downloaded firmware to flash.
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

      // Finalize the update and reboot.
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
}

void WebServer::begin()
{
  // Register all the URL handlers with the server.
  server.on("/", HTTP_GET, onRootRequest);
  server.on("/wifi", HTTP_GET, onWifiRequest);
  server.on("/wifi/save", HTTP_POST, onWifiSaveRequest);
  server.on("/update", HTTP_GET, onUpdateRequest);
  server.on("/update/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, onUpdateUpload);
  server.on("/update/github", HTTP_GET, onGithubUpdate);

  // --- Settings Page ---
  // Handle GET requests for the settings page.
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (isUpdating) return;
    
    auto& config = ConfigManager::getInstance();
    String page = SETTINGS_PAGE_HTML;

    // Populate the page template with current settings.
    page.replace("%HEAD%", BOOTSTRAP_HEAD);
    page.replace("%AUTO_BRIGHTNESS_CHECKED%", config.isAutoBrightness() ? "checked" : "");
    page.replace("%BRIGHTNESS_VALUE%", String(config.getBrightness()));
    page.replace("%IS_24_HOUR_CHECKED%", config.is24HourFormat() ? "checked" : "");
    page.replace("%USE_CELSIUS_CHECKED%", config.isCelsius() ? "checked" : "");

    request->send(200, "text/html", page); });

  // Handle POST requests to save settings.
  server.on("/settings/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (isUpdating) return;

    // Update the configuration with the submitted form data.
    auto& config = ConfigManager::getInstance();
    config.setAutoBrightness(request->hasParam("autoBrightness"));
    config.setBrightness(request->arg("brightness").toInt());
    config.set24HourFormat(request->hasParam("use24HourFormat"));
    config.setCelsius(request->hasParam("useCelsius"));
    
    config.save();

    request->send(200, "text/plain", "Settings saved successfully!"); });

  // Start the web server.
  server.begin();
}