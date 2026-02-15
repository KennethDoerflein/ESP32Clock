#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "FirmwareVerifier.h"
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
 * This singleton class handles the Over-the-Air (OTA) update process with
 * cryptographic verification. It provides methods to process firmware chunks
 * from a file upload and to initiate an update from a GitHub release.
 *
 * Security features:
 * - Ed25519 signature verification
 * - SHA-256 hash verification
 * - Secure TLS connections (no setInsecure())
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
     * @brief Sets the signature for verifying the uploaded firmware.
     * @param signatureHex Hex-encoded Ed25519 signature (128 chars).
     * @return True if signature was parsed successfully.
     */
    bool setUploadSignature(const char *signatureHex);

    /**
     * @brief Finalizes the update process with verification.
     * @return True if the update was successful, false otherwise.
     */
    bool endUpdate();

    /**
     * @brief Initiates a firmware update from GitHub with signature verification.
     * @return A string indicating the result of the update check.
     */
    String handleGithubUpdate();

    /**
     * @brief Checks if an update is currently in progress.
     * @return True if an update is in progress, false otherwise.
     */
    bool isUpdateInProgress();

    /**
     * @brief Gets the last verification error message.
     * @return Error message or empty string if no error.
     */
    String getLastError() const { return _lastError; }

private:
    /**
     * @brief Private constructor to enforce the singleton pattern.
     */
    UpdateManager();
    UpdateManager(const UpdateManager &) = delete;
    UpdateManager &operator=(const UpdateManager &) = delete;

    bool _updateFailed = false;
    bool _updateInProgress = false;
    String _lastError;

    // Signature verification for uploads
    uint8_t _uploadSignature[FirmwareVerifier::ED25519_SIGNATURE_SIZE];
    bool _hasUploadSignature = false;
    FirmwareVerifier::SHA256Context _uploadHashCtx;
    uint8_t _uploadHash[FirmwareVerifier::SHA256_HASH_SIZE];

    // GitHub update data structure
    struct GithubUpdateInfo
    {
        String firmwareUrl;
        String signatureUrl;
        String checksumUrl;
    };

    static void runGithubUpdateTask(void *pvParameters);
    bool downloadAndVerifyFile(WiFiClientSecure &client, const String &url,
                               uint8_t *buffer, size_t bufferSize, size_t &bytesRead);
    bool verifyFirmwareSignature(const uint8_t *firmwareData, size_t firmwareLen,
                                  const uint8_t *signature);
};
