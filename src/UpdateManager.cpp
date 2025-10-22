#include "UpdateManager.h"
#include <Update.h>
#include <ArduinoJson.h>
#include "version.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "github_ca.h"
#include "SerialLog.h"

UpdateManager &UpdateManager::getInstance()
{
  static UpdateManager instance;
  return instance;
}

// Private constructor
UpdateManager::UpdateManager() {}

void UpdateManager::handleFileUpload(uint8_t *data, size_t len, size_t index, size_t total)
{
  // If a previous write failed, don't continue
  if (_updateFailed)
  {
    return;
  }

  if (index == 0)
  {
    SerialLog::getInstance().print("Update Start\n");
    _updateInProgress = true;
    _updateFailed = false; // Reset flag for new update
    // For multipart forms, the total size is unknown, so use UPDATE_SIZE_UNKNOWN
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
      _updateFailed = true;
    }
  }

  // Abort if begin() or a previous write has already failed
  if (_updateFailed)
  {
    return;
  }

  if (Update.write(data, len) != len)
  {
    Update.printError(Serial);
    _updateFailed = true; // Set flag on write failure
  }
}

bool UpdateManager::endUpdate()
{
  bool success = false;
  if (_updateFailed)
  {
    SerialLog::getInstance().print("Update failed. Not finalizing.\n");
    Update.abort();
  }
  else if (Update.end(true))
  { // true to finalize the update
    SerialLog::getInstance().print("Update Success\n");
    success = true;
  }
  else
  {
    Update.printError(Serial);
  }

  _updateFailed = false; // Reset state for the next update attempt
  _updateInProgress = false;
  return success;
}

bool UpdateManager::isUpdateInProgress()
{
  return _updateInProgress;
}

String UpdateManager::handleGithubUpdate()
{
  if (_updateInProgress)
  {
    return "An update is already in progress.";
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);

  String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/releases/latest";
  http.begin(client, url);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    String errorMsg = "Error checking for updates. HTTP code: " + String(httpCode) + " " + http.errorToString(httpCode);
    http.end();
    return errorMsg;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    SerialLog::getInstance().printf("deserializeJson() failed: %s\n", error.c_str());
    return "Error parsing update data.";
  }

  if (doc["tag_name"].isNull())
  {
    return "Could not find 'tag_name' in release data.";
  }

  const char *tagName = doc["tag_name"];
  if (strcmp(tagName, FIRMWARE_VERSION) == 0)
  {
    return "No new update found.";
  }

  SerialLog::getInstance().printf("Current version: %s, New version: %s\n", FIRMWARE_VERSION, tagName);

  JsonArray assets = doc["assets"];
  for (JsonObject asset : assets)
  {
    String assetName = asset["name"];
    if (assetName.endsWith(".bin"))
    {
      String *downloadUrl = new String(asset["browser_download_url"].as<String>());

      BaseType_t taskCreated = xTaskCreate(
          &UpdateManager::runGithubUpdateTask,
          "github_update_task",
          16384,               // Stack size
          (void *)downloadUrl, // Task parameter
          5,                   // Priority
          NULL                 // Task handle
      );

      if (taskCreated != pdPASS)
      {
        SerialLog::getInstance().print("Failed to create update task.\n");
        delete downloadUrl; // IMPORTANT: Clean up memory if task creation fails
        return "Failed to start update process.";
      }

      return "New version found! Starting update...";
    }
  }

  return "Update found, but no .bin file in release assets.";
}

void UpdateManager::runGithubUpdateTask(void *pvParameters)
{
  String *downloadUrl = (String *)pvParameters;

  getInstance()._updateInProgress = true;

  HTTPClient http;
  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);

  http.begin(client, *downloadUrl);

  // Collect Location header for redirect handling
  const char *headerKeys[] = {"Location"};
  http.collectHeaders(headerKeys, 1);

  int httpCode = http.GET();

  // Handle redirects
  if (httpCode > 0 && (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_TEMPORARY_REDIRECT))
  {
    String newUrl = http.header("Location");
    SerialLog::getInstance().print("Redirecting to: " + newUrl + "\n");
    if (newUrl.isEmpty())
    {
      SerialLog::getInstance().print("Redirect location is empty!\n");
    }
    else
    {
      http.end();
      client.stop(); // Explicitly stop the client to clear connection state
      // The redirected URL is on a different domain, so we won't have the
      // correct CA certificate. The initial request to the trusted GitHub API
      // server was secure, so we can proceed without a certificate for the
      // download itself.
      client.setInsecure();
      http.begin(client, newUrl);
      httpCode = http.GET();
    }
  }

  if (httpCode == HTTP_CODE_OK)
  {
    int len = http.getSize();
    if (!Update.begin(len))
    {
      Update.printError(Serial);
    }
    else
    {
      WiFiClient *stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);
      if (written > 0)
      {
        SerialLog::getInstance().printf("Written %d bytes\n", written);
      }
      else
      {
        SerialLog::getInstance().print("Write failed\n");
      }

      if (Update.end())
      {
        if (Update.isFinished())
        {
          SerialLog::getInstance().print("Update successful! Rebooting...\n");
          ESP.restart();
        }
        else
        {
          SerialLog::getInstance().print("Update not finished. Something went wrong.\n");
        }
      }
      else
      {
        Update.printError(Serial);
      }
    }
  }
  else
  {
    SerialLog::getInstance().printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  delete downloadUrl; // Free the memory for the URL
  getInstance()._updateInProgress = false;
  vTaskDelete(NULL); // Delete the task
}