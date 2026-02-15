/**
 * @file FirmwareVerifier.cpp
 * @brief Implementation of cryptographic firmware verification.
 *
 * Uses libsodium for Ed25519 signature verification - the industry-standard
 * crypto library officially maintained by Espressif for ESP32. Uses ESP32's
 * mbedTLS for SHA-256 hashing. This ensures firmware updates are authenticated
 * before being applied, preventing supply chain attacks.
 */

#include "FirmwareVerifier.h"
#include "mbedtls/sha256.h"
#include "SerialLog.h"
#include "sodium.h"

bool FirmwareVerifier::computeSHA256(const uint8_t *data, size_t len, uint8_t *hashOut)
{
    if (!data || !hashOut)
    {
        return false;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256, not SHA-224
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, hashOut);
    mbedtls_sha256_free(&ctx);

    return true;
}

FirmwareVerifier::SHA256Context::SHA256Context() : _ctx(nullptr)
{
    _ctx = new mbedtls_sha256_context;
    mbedtls_sha256_init((mbedtls_sha256_context *)_ctx);
}

FirmwareVerifier::SHA256Context::~SHA256Context()
{
    if (_ctx)
    {
        mbedtls_sha256_free((mbedtls_sha256_context *)_ctx);
        delete (mbedtls_sha256_context *)_ctx;
    }
}

bool FirmwareVerifier::SHA256Context::begin()
{
    if (!_ctx)
        return false;
    mbedtls_sha256_starts((mbedtls_sha256_context *)_ctx, 0);
    return true;
}

bool FirmwareVerifier::SHA256Context::update(const uint8_t *data, size_t len)
{
    if (!_ctx || !data)
        return false;
    mbedtls_sha256_update((mbedtls_sha256_context *)_ctx, data, len);
    return true;
}

bool FirmwareVerifier::SHA256Context::finish(uint8_t *hashOut)
{
    if (!_ctx || !hashOut)
        return false;
    mbedtls_sha256_finish((mbedtls_sha256_context *)_ctx, hashOut);
    return true;
}

bool FirmwareVerifier::verifySignature(const uint8_t *hash, const uint8_t *signature,
                                        const uint8_t *publicKey)
{
    if (!hash || !signature || !publicKey)
    {
        SerialLog::getInstance().print("Signature verification failed: null parameter\n");
        return false;
    }

    // Ed25519 verification using libsodium
    // crypto_sign_verify_detached returns 0 on success, -1 on failure
    int result = crypto_sign_verify_detached(signature, hash, SHA256_HASH_SIZE, publicKey);

    if (result != 0)
    {
        SerialLog::getInstance().print("Signature verification failed: invalid signature\n");
        return false;
    }

    SerialLog::getInstance().print("Signature verification successful\n");
    return true;
}

uint8_t FirmwareVerifier::hexCharToNibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return 0xFF;
}

bool FirmwareVerifier::parseHexSignature(const char *hexStr, uint8_t *sigOut)
{
    if (!hexStr || !sigOut)
        return false;

    size_t len = strlen(hexStr);
    if (len != ED25519_SIGNATURE_SIZE * 2)
    {
        SerialLog::getInstance().printf("Invalid signature hex length: %d (expected %d)\n",
                                         len, ED25519_SIGNATURE_SIZE * 2);
        return false;
    }

    for (size_t i = 0; i < ED25519_SIGNATURE_SIZE; i++)
    {
        uint8_t high = hexCharToNibble(hexStr[i * 2]);
        uint8_t low = hexCharToNibble(hexStr[i * 2 + 1]);
        if (high == 0xFF || low == 0xFF)
        {
            SerialLog::getInstance().print("Invalid hex character in signature\n");
            return false;
        }
        sigOut[i] = (high << 4) | low;
    }

    return true;
}

bool FirmwareVerifier::parseHexHash(const char *hexStr, uint8_t *hashOut)
{
    if (!hexStr || !hashOut)
        return false;

    size_t len = strlen(hexStr);
    if (len != SHA256_HASH_SIZE * 2)
    {
        SerialLog::getInstance().printf("Invalid hash hex length: %d (expected %d)\n",
                                         len, SHA256_HASH_SIZE * 2);
        return false;
    }

    for (size_t i = 0; i < SHA256_HASH_SIZE; i++)
    {
        uint8_t high = hexCharToNibble(hexStr[i * 2]);
        uint8_t low = hexCharToNibble(hexStr[i * 2 + 1]);
        if (high == 0xFF || low == 0xFF)
        {
            SerialLog::getInstance().print("Invalid hex character in hash\n");
            return false;
        }
        hashOut[i] = (high << 4) | low;
    }

    return true;
}

String FirmwareVerifier::toHexString(const uint8_t *data, size_t len)
{
    String result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; i++)
    {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", data[i]);
        result += hex;
    }
    return result;
}
