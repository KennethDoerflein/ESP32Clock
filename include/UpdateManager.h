#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

#define GITHUB_REPO "KennethDoerflein/ESP32Clock"

/**
 * @class UpdateManager
 * @brief Manages firmware updates from file uploads and GitHub releases.
 *
 * This singleton class handles the Over-the-Air (OTA) update process. It
 * provides methods to process firmware chunks from a file upload and to
 * initiate an update from a GitHub release.
 */
class UpdateManager
{
public:
  /**
   * @brief Gets the singleton instance of the UpdateManager.
   * @return A reference to the singleton UpdateManager instance.
   */
  static UpdateManager &getInstance();

  /**
   * @brief Processes a chunk of the firmware file during an upload.
   * @param data A pointer to the data chunk.
   * @param len The length of the data chunk.
   * @param index The starting index of the chunk.
   * @param total The total size of the firmware file.
   */
  void handleFileUpload(uint8_t *data, size_t len, size_t index, size_t total);

  /**
   * @brief Finalizes the update process.
   * @return True if the update was successful, false otherwise.
   */
  bool endUpdate();

  /**
   * @brief Initiates a firmware update from GitHub.
   * @return A string indicating the result of the update check (e.g., "OK", "No update").
   */
  String handleGithubUpdate();

  /**
   * @brief Checks if an update is currently in progress.
   * @return True if an update is in progress, false otherwise.
   */
  bool isUpdateInProgress();

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  UpdateManager();
  UpdateManager(const UpdateManager &) = delete;
  UpdateManager &operator=(const UpdateManager &) = delete;

  bool _updateFailed = false;
  bool _updateInProgress = false;

  static void runGithubUpdateTask(void *pvParameters);
};