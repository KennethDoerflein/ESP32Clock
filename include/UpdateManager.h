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

class UpdateManager
{
public:
  static UpdateManager &getInstance();

  void handleFileUpload(uint8_t *data, size_t len, size_t index, size_t total);
  bool endUpdate();
  String handleGithubUpdate();
  bool isUpdateInProgress();

private:
  UpdateManager();
  UpdateManager(const UpdateManager &) = delete;
  UpdateManager &operator=(const UpdateManager &) = delete;

  bool _updateFailed = false;
  bool _updateInProgress = false;

  static void runGithubUpdateTask(void *pvParameters);
};