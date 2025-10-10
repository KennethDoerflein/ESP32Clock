// OtaManager.cpp

#include "OtaManager.h"
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "pages.h" // For UPDATE_PAGE_HTML

// --- GitHub OTA Configuration ---
#define GITHUB_REPO "KennethDoerflein/ESP32Clock"
#define FIRMWARE_ASSET_NAME "firmware.bin"

// --- Singleton Implementation ---
OtaManager &OtaManager::getInstance()
{
  static OtaManager instance;
  return instance;
}

// --- Constructor ---
OtaManager::OtaManager() : _isUpdating(false), _processor(nullptr) {}

// --- Public Methods ---

bool OtaManager::isUpdating() const
{
  return _isUpdating;
}

void OtaManager::begin(AsyncWebServer &server, TplProcessor processor)
{
  _processor = processor;

  server.on("/update", HTTP_GET, std::bind(&OtaManager::onUpdateRequest, this, std::placeholders::_1));
  server.on(
      "/update/upload", HTTP_POST,
      [this](AsyncWebServerRequest *request)
      {
        // This is called after the upload is complete.
        // The 'final' flag in onUpdateUpload handles the response.
      },
      std::bind(&OtaManager::onUpdateUpload, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
  server.on("/update/github", HTTP_GET, std::bind(&OtaManager::onGithubUpdate, this, std::placeholders::_1));
}

// --- Private Methods ---

void OtaManager::onUpdateRequest(AsyncWebServerRequest *request)
{
  if (_isUpdating)
  {
    request->send(503, "text/plain", "Update in progress");
    return;
  }
  request->send_P(200, "text/html", UPDATE_PAGE_HTML, _processor);
}

void OtaManager::onUpdateUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index) // This block runs only on the first chunk of the upload.
  {
    if (_isUpdating)
    {
      request->send(503, "text/plain", "Update already in progress.");
      return;
    }
    _isUpdating = true;
    Serial.printf("Update Start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
      _isUpdating = false;
    }
  }

  if (!_isUpdating)
    return;

  if (len) // Write the current chunk of data.
  {
    if (Update.write(data, len) != len)
    {
      Update.printError(Serial);
      _isUpdating = false;
    }
  }

  if (final) // This block runs only on the last chunk.
  {
    if (Update.end(true))
    {
      Serial.println("Update Success");
      request->send(200, "text/plain", "Update successful! Rebooting...");
      delay(1000);
      ESP.restart();
    }
    else
    {
      Update.printError(Serial);
      request->send(500, "text/plain", "Update failed");
      _isUpdating = false;
    }
  }
}

void OtaManager::onGithubUpdate(AsyncWebServerRequest *request)
{
  if (_isUpdating)
  {
    request->send(503, "text/plain", "Update already in progress.");
    return;
  }
  _isUpdating = true;

  HTTPClient http;
  WiFiClient client;
  String url = "https://api.github.com/repos/" GITHUB_REPO "/releases/latest";

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    _isUpdating = false;
    request->send(500, "text/plain", "Failed to fetch release info from GitHub.");
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
    _isUpdating = false;
    request->send(404, "text/plain", "Firmware asset not found in the latest release.");
    return;
  }

  request->send(200, "text/plain", "Found update. Starting download...");

  HTTPClient firmwareHttp;
  firmwareHttp.begin(client, firmwareUrl);
  int firmwareHttpCode = firmwareHttp.GET();

  if (firmwareHttpCode == HTTP_CODE_OK)
  {
    int contentLength = firmwareHttp.getSize();
    if (contentLength <= 0)
    {
      firmwareHttp.end();
      _isUpdating = false;
      request->send(500, "text/plain", "GitHub file is empty.");
      return;
    }

    if (!Update.begin(contentLength))
    {
      Update.printError(Serial);
      firmwareHttp.end();
      _isUpdating = false;
      request->send(500, "text/plain", "Not enough space to begin OTA");
      return;
    }

    size_t written = Update.writeStream(firmwareHttp.getStream());

    if (written != contentLength)
    {
      Update.printError(Serial);
      firmwareHttp.end();
      _isUpdating = false;
      request->send(500, "text/plain", "OTA write failed.");
      return;
    }

    if (Update.end(true))
    {
      Serial.println("GitHub OTA successful! Rebooting...");
      delay(1000);
      ESP.restart();
    }
    else
    {
      Update.printError(Serial);
      _isUpdating = false;
      request->send(500, "text/plain", "Update failed to commit.");
    }
  }
  else
  {
    _isUpdating = false;
    request->send(firmwareHttpCode, "text/plain", "Failed to download firmware.");
  }
  firmwareHttp.end();
}