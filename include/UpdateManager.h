#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "version.h"

#define GITHUB_REPO "KennethDoerflein/ESP32Clock"

class UpdateManager
{
public:
  static UpdateManager &getInstance();

  void handleFileUpload(uint8_t *data, size_t len, size_t index, size_t total);
  bool endUpdate();
  String handleGithubUpdate();

private:
  UpdateManager();
  UpdateManager(const UpdateManager &) = delete;
  UpdateManager &operator=(const UpdateManager &) = delete;

  bool _updateFailed = false;

  static void runGithubUpdateTask(void *pvParameters);
};