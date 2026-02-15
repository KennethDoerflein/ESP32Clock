#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @class FirmwareVerifier
 * @brief Provides cryptographic verification of OTA firmware updates.
 *
 * Uses Ed25519 signatures and SHA-256 hashing to verify firmware integrity
 * and authenticity before updates are applied. This prevents supply chain
 * attacks by ensuring only properly signed firmware can be installed.
 */
class FirmwareVerifier
{
public:
    static constexpr size_t SHA256_HASH_SIZE = 32;
    static constexpr size_t ED25519_SIGNATURE_SIZE = 64;
    static constexpr size_t ED25519_PUBLIC_KEY_SIZE = 32;

    /**
     * @brief Computes the SHA-256 hash of firmware data.
     * @param data Pointer to the firmware data.
     * @param len Length of the firmware data in bytes.
     * @param hashOut Buffer to store the 32-byte hash (must be at least SHA256_HASH_SIZE).
     * @return True if hash computed successfully, false otherwise.
     */
    static bool computeSHA256(const uint8_t *data, size_t len, uint8_t *hashOut);

    /**
     * @brief Computes SHA-256 hash incrementally using streaming.
     * Useful for large firmware files that cannot fit in memory.
     */
    class SHA256Context
    {
    public:
        SHA256Context();
        ~SHA256Context();
        bool begin();
        bool update(const uint8_t *data, size_t len);
        bool finish(uint8_t *hashOut);

    private:
        void *_ctx;
    };

    /**
     * @brief Verifies an Ed25519 signature against a hash.
     * @param hash The 32-byte SHA-256 hash of the firmware.
     * @param signature The 64-byte Ed25519 signature.
     * @param publicKey The 32-byte Ed25519 public key.
     * @return True if signature is valid, false otherwise.
     */
    static bool verifySignature(const uint8_t *hash, const uint8_t *signature,
                                 const uint8_t *publicKey);

    /**
     * @brief Parses a hex-encoded signature string into binary.
     * @param hexStr The hex string (128 characters for 64 bytes).
     * @param sigOut Buffer to store the 64-byte signature.
     * @return True if parsing succeeded, false otherwise.
     */
    static bool parseHexSignature(const char *hexStr, uint8_t *sigOut);

    /**
     * @brief Parses a hex-encoded SHA-256 hash string into binary.
     * @param hexStr The hex string (64 characters for 32 bytes).
     * @param hashOut Buffer to store the 32-byte hash.
     * @return True if parsing succeeded, false otherwise.
     */
    static bool parseHexHash(const char *hexStr, uint8_t *hashOut);

    /**
     * @brief Converts binary data to hex string.
     * @param data Binary data to convert.
     * @param len Length of binary data.
     * @return Hex-encoded string.
     */
    static String toHexString(const uint8_t *data, size_t len);

private:
    static uint8_t hexCharToNibble(char c);
};
