/**
 * @file UpdateManager.cpp
 * @brief Implements the UpdateManager class for handling firmware updates.
 *
 * This file contains the implementation for managing Over-the-Air (OTA) updates.
 * It handles updates from both file uploads via the web interface and direct
 * downloads from GitHub releases.
 */
#include "UpdateManager.h"
#include <Update.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "github_ca.h"
#include "SerialLog.h"
#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

/**
 * @brief Gets the singleton instance of the UpdateManager.
 * @return A reference to the singleton instance.
 */
UpdateManager &UpdateManager::getInstance()
{
  static UpdateManager instance;
  return instance;
}

/**
 * @brief Private constructor to enforce the singleton pattern.
 */
UpdateManager::UpdateManager() {}

/**
 * @brief Processes a chunk of firmware data from a file upload.
 *
 * This function is called repeatedly by the web server's upload handler.
 * It initializes the update process on the first chunk and writes subsequent
 * chunks to the flash memory.
 *
 * @param data Pointer to the data chunk.
 * @param len Length of the data chunk.
 * @param index Starting index of the chunk in the total file.
 * @param total Total size of the firmware file.
 */
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

/**
 * @brief Finalizes the firmware update.
 *
 * This should be called after the last chunk of data has been received.
 * It checks if the update was successful and either commits the changes
 * or aborts the process.
 *
 * @return True if the update was successful, false otherwise.
 */
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

/**
 * @brief Checks if an update is currently in progress.
 * @return True if an update is in progress, false otherwise.
 */
bool UpdateManager::isUpdateInProgress()
{
  return _updateInProgress;
}

/**
 * @brief Checks for updates on GitHub and initiates the update process.
 *
 * This function fetches the latest release information from the GitHub API,
 * compares the tag name with the current firmware version, and if a newer
 * version is found, it creates a new FreeRTOS task to download and apply
 * the update in the background.
 *
 * @return A string indicating the status of the update check.
 */
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

/**
 * @brief FreeRTOS task to perform the GitHub update.
 *
 * This function runs in a separate task to avoid blocking the main application
 * loop during the download and flashing process. It handles the HTTP download,
 * including redirects, and writes the firmware to flash.
 *
 * @param pvParameters A void pointer to a String object containing the download URL.
 */
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