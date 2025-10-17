#include "UpdateManager.h"
#include <Update.h>
#include <ArduinoJson.h>
#include "version.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "github_ca.h"

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
    Serial.println("Update Start");
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
    Serial.println("Update failed. Not finalizing.");
    Update.abort();
  }
  else if (Update.end(true))
  { // true to finalize the update
    Serial.println("Update Success");
    success = true;
  }
  else
  {
    Update.printError(Serial);
  }

  _updateFailed = false; // Reset state for the next update attempt
  return success;
}

String UpdateManager::handleGithubUpdate()
{
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
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
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

  Serial.printf("Current version: %s, New version: %s\n", FIRMWARE_VERSION, tagName);

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
          8192,                // Stack size
          (void *)downloadUrl, // Task parameter
          5,                   // Priority
          NULL                 // Task handle
      );

      if (taskCreated != pdPASS)
      {
        Serial.println("Failed to create update task.");
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

  HTTPClient http;
  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);

  http.begin(client, *downloadUrl);
  int httpCode = http.GET();

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
      uint8_t buff[1024] = {0};
      size_t written = 0;
      while (http.connected() && (len > 0 || len == -1))
      {
        size_t size = stream->available();
        if (size)
        {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          if (Update.write(buff, c) != c)
          {
            Update.printError(Serial);
            break; // Exit loop on write error
          }
          written += c;
        }
        delay(1); // Yield to other tasks
      }

      if (Update.end())
      {
        if (Update.isFinished())
        {
          Serial.println("Update successful! Rebooting...");
          ESP.restart();
        }
        else
        {
          Serial.println("Update not finished. Something went wrong.");
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
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  delete downloadUrl; // Free the memory for the URL
  vTaskDelete(NULL);  // Delete the task
}