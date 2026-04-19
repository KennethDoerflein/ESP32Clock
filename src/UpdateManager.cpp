/**
 * @file UpdateManager.cpp
 * @brief Implements the UpdateManager class for handling secure firmware updates.
 *
 * This file contains the implementation for managing Over-the-Air (OTA) updates
 * with cryptographic signature verification to prevent supply chain attacks.
 * Updates are verified using Ed25519 signatures before being applied.
 */
#include "UpdateManager.h"
#include <esp_task_wdt.h>
#include "FirmwareVerifier.h"
#include "ota_public_key.h"
#include <Update.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "NtpSync.h"
#include "SerialLog.h"
#if __has_include("version.h")
#include "version.h"
#else
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
    if (_updateFailed)
    {
        return;
    }

    if (index == 0)
    {
        SerialLog::getInstance().print("Update Start\n");
        _updateInProgress = true;
        _updateFailed = false;
        _lastError = "";


        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            Update.printError(Serial);
            _updateFailed = true;
            _lastError = "Update.begin() failed";
        }
    }

    if (_updateFailed)
    {
        return;
    }


    if (Update.write(data, len) != len)
    {
        Update.printError(Serial);
        _updateFailed = true;
        _lastError = "Write failed";
    }
}


/**
 * @brief Finalizes the update process with verification.
 * @return True if the update was successful, false otherwise.
 */
bool UpdateManager::endUpdate()
{
    bool success = false;

    if (_updateFailed)
    {
        SerialLog::getInstance().print("Update failed. Not finalizing.\n");
        Update.abort();
        goto cleanup;
    }


    if (Update.end(true))
    {
        SerialLog::getInstance().print("Update Success\n");
        success = true;
    }
    else
    {
        Update.printError(Serial);
        _lastError = "Update.end() failed";
    }

cleanup:
    _updateFailed = false;
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
 * @brief Initiates a firmware update from GitHub with signature verification.
 * @return A string indicating the result of the update check.
 */
String UpdateManager::handleGithubUpdate()
{
    if (_updateInProgress)
    {
        return "An update is already in progress.";
    }

    // Ensure system time is set for TLS verification
    time_t now = time(nullptr);
    if (now < 1000000000) // Approx year 2001
    {
        SerialLog::getInstance().print("Time not set. Attempting NTP sync...\n");
        // Use a local ntp sync helper to avoid recursion if syncTime calls getNTPData which calls configTime
        if (!syncTime())
        {
            return "Failed to synchronize time. Cannot verify certificates.";
        }
        now = time(nullptr);
    }
    
    char timeStr[64];
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    SerialLog::getInstance().printf("System time: %s\n", timeStr);

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

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

    // Look for firmware.bin, firmware.sig, and firmware.sha256
    GithubUpdateInfo *updateInfo = new GithubUpdateInfo();
    JsonArray assets = doc["assets"];

    for (JsonObject asset : assets)
    {
        String assetName = asset["name"].as<String>();
        String downloadUrl = asset["browser_download_url"].as<String>();

        if (assetName == "firmware.bin")
        {
            updateInfo->firmwareUrl = downloadUrl;
        }
        else if (assetName == "firmware.sig")
        {
            updateInfo->signatureUrl = downloadUrl;
        }
        else if (assetName == "firmware.sha256")
        {
            updateInfo->checksumUrl = downloadUrl;
        }
    }

    if (updateInfo->firmwareUrl.isEmpty())
    {
        delete updateInfo;
        return "Update found, but no firmware.bin in release assets.";
    }

    // Check if signature is available when OTA key is configured
    if (OTA_KEY_CONFIGURED && updateInfo->signatureUrl.isEmpty())
    {
        delete updateInfo;
        return "SECURITY: Update rejected - no signature file (firmware.sig) found.";
    }

    // Set flag before creating task so UI locks immediately
    _updateInProgress = true;

    BaseType_t taskCreated = xTaskCreate(
        &UpdateManager::runGithubUpdateTask,
        "github_update_task",
        16384,
        (void *)updateInfo,
        5,
        NULL);

    if (taskCreated != pdPASS)
    {
        SerialLog::getInstance().print("Failed to create update task.\n");
        delete updateInfo;
        _updateInProgress = false;
        return "Failed to start update process.";
    }

    return "New version found! Starting secure update...";
}

/**
 * @brief Task function to handle the background GitHub firmware update.
 * @param pvParameters Pointer to a GithubUpdateInfo struct.
 */
void UpdateManager::runGithubUpdateTask(void *pvParameters)
{
    // Enroll in Task Watchdog
    esp_task_wdt_add(NULL);
    
    GithubUpdateInfo *updateInfo = (GithubUpdateInfo *)pvParameters;
    getInstance()._updateInProgress = true;
    getInstance()._lastError = "";

    // Download signature file first (if OTA key is configured)
    uint8_t signature[FirmwareVerifier::ED25519_SIGNATURE_SIZE];
    bool hasSignature = false;

    if (OTA_KEY_CONFIGURED && !updateInfo->signatureUrl.isEmpty())
    {
        HTTPClient http;
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(10000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        SerialLog::getInstance().printf("Connecting to: %s\n", updateInfo->signatureUrl.c_str());
        http.begin(client, updateInfo->signatureUrl);

        int httpCode = http.GET();
        SerialLog::getInstance().printf("HTTP Response: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK)
        {
            String sigContent = http.getString();
            sigContent.trim();

            if (FirmwareVerifier::parseHexSignature(sigContent.c_str(), signature))
            {
                hasSignature = true;
                SerialLog::getInstance().print("Signature file downloaded successfully\n");
            }
            else
            {
                SerialLog::getInstance().print("Failed to parse signature file\n");
            }
        }
        else
        {
            SerialLog::getInstance().printf("Failed to download signature: HTTP %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
        }
        http.end();

        if (!hasSignature)
        {
            SerialLog::getInstance().print("SECURITY: Cannot proceed without valid signature\n");
            getInstance()._lastError = "Failed to download or parse signature file";
            delete updateInfo;
            getInstance()._updateInProgress = false;
            vTaskDelete(NULL);
            return;
        }
    }

    // Create fresh client objects for firmware download
    WiFiClientSecure firmwareClient;
    HTTPClient firmwareHttp;
    
    firmwareClient.setInsecure();
    firmwareClient.setTimeout(10000);
    firmwareHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    SerialLog::getInstance().printf("Connecting to: %s\n", updateInfo->firmwareUrl.c_str());
    firmwareHttp.begin(firmwareClient, updateInfo->firmwareUrl);

    int httpCode = firmwareHttp.GET();
    SerialLog::getInstance().printf("HTTP Response: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
        int contentLength = firmwareHttp.getSize();
        SerialLog::getInstance().printf("Firmware size: %d bytes\n", contentLength);

        WiFiClient *stream = firmwareHttp.getStreamPtr();

        // Allocate buffer for firmware (we need it for signature verification)
        // For large firmware, we'd use streaming hash, but this is simpler for now
        uint8_t *firmwareBuffer = nullptr;
        size_t firmwareLen = 0;

        if (OTA_KEY_CONFIGURED && hasSignature)
        {
            // Need to buffer firmware for signature verification
            // Use PSRAM for the large firmware buffer to avoid fragmenting internal heap
            firmwareBuffer = (uint8_t *)ps_malloc(contentLength);
            if (!firmwareBuffer)
            {
                SerialLog::getInstance().print("Failed to allocate firmware buffer\n");
                getInstance()._lastError = "Out of memory for firmware buffer";
                firmwareHttp.end();
                delete updateInfo;
                getInstance()._updateInProgress = false;
                vTaskDelete(NULL);
                return;
            }

            // Read entire firmware into buffer
            size_t bytesRead = 0;
            while (bytesRead < (size_t)contentLength && firmwareHttp.connected())
            {
                esp_task_wdt_reset(); // Feed watchdog during download
                size_t available = stream->available();
                if (available > 0)
                {
                    size_t toRead = min(available, (size_t)contentLength - bytesRead);
                    size_t read = stream->readBytes(firmwareBuffer + bytesRead, toRead);
                    bytesRead += read;

                    if (bytesRead % 65536 == 0)
                    {
                        SerialLog::getInstance().printf("Downloaded %d / %d bytes\n", bytesRead, contentLength);
                    }
                }
                delay(1);
            }
            firmwareLen = bytesRead;

            // Compute hash and verify signature
            uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
            if (!FirmwareVerifier::computeSHA256(firmwareBuffer, firmwareLen, hash))
            {
                SerialLog::getInstance().print("Failed to compute firmware hash\n");
                free(firmwareBuffer);
                firmwareHttp.end();
                delete updateInfo;
                getInstance()._updateInProgress = false;
                vTaskDelete(NULL);
                return;
            }

            String hashHex = FirmwareVerifier::toHexString(hash, FirmwareVerifier::SHA256_HASH_SIZE);
            SerialLog::getInstance().printf("Firmware SHA-256: %s\n", hashHex.c_str());

            if (!FirmwareVerifier::verifySignature(hash, signature, OTA_PUBLIC_KEY))
            {
                SerialLog::getInstance().print("SECURITY: Signature verification FAILED!\n");
                SerialLog::getInstance().print("Firmware may have been tampered with. Update rejected.\n");
                getInstance()._lastError = "Signature verification failed";
                free(firmwareBuffer);
                firmwareHttp.end();
                delete updateInfo;
                getInstance()._updateInProgress = false;
                vTaskDelete(NULL);
                return;
            }

            SerialLog::getInstance().print("Signature verification PASSED - firmware is authentic\n");

            // Now write verified firmware
            if (!Update.begin(firmwareLen))
            {
                Update.printError(Serial);
                free(firmwareBuffer);
                firmwareHttp.end();
                delete updateInfo;
                getInstance()._updateInProgress = false;
                vTaskDelete(NULL);
                return;
            }

            size_t written = Update.write(firmwareBuffer, firmwareLen);
            free(firmwareBuffer);

            if (written != firmwareLen)
            {
                SerialLog::getInstance().printf("Write failed: wrote %d of %d\n", written, firmwareLen);
                Update.abort();
                firmwareHttp.end();
                delete updateInfo;
                getInstance()._updateInProgress = false;
                vTaskDelete(NULL);
                return;
            }
        }
        else
        {
            // No signature verification (key not configured) - stream directly
            SerialLog::getInstance().print("WARNING: Updating without signature verification\n");

            if (!Update.begin(contentLength))
            {
                Update.printError(Serial);
                firmwareHttp.end();
                delete updateInfo;
                getInstance()._updateInProgress = false;
                vTaskDelete(NULL);
                return;
            }

            size_t written = Update.writeStream(*stream);
            if (written != (size_t)contentLength)
            {
                SerialLog::getInstance().printf("Write failed: wrote %d of %d\n", written, contentLength);
            }
        }

        if (Update.end())
        {
            if (Update.isFinished())
            {
                SerialLog::getInstance().print("Update successful! Rebooting...\n");
                firmwareHttp.end();
                delete updateInfo;
                delay(1000); // Give system time to flush logs before restart
                ESP.restart();
                // Never reached, but include for safety
                vTaskDelete(NULL);
            }
            else
            {
                SerialLog::getInstance().print("Update not finished. Something went wrong.\n");
                getInstance()._updateInProgress = false;
            }
        }
        else
        {
            Update.printError(Serial);
            getInstance()._updateInProgress = false;
        }
    }
    else
    {
        SerialLog::getInstance().printf("HTTP GET failed, error: %s\n", firmwareHttp.errorToString(httpCode).c_str());
        getInstance()._updateInProgress = false;
    }

    firmwareHttp.end();
    delete updateInfo;
    
    esp_task_wdt_delete(NULL); // Remove from watchdog before deleting task
    vTaskDelete(NULL);
}