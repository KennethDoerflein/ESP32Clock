#include <unity.h>
#include "FirmwareVerifier.h"
#include "../../src/FirmwareVerifier.cpp"
#include <sodium.h>

// Minimal SerialLog stubs to isolate unit test from LittleFS / WebSockets
SerialLog::SerialLog() : _ws("/ws/log") {}
SerialLog &SerialLog::getInstance() {
    static SerialLog instance;
    return instance;
}
void SerialLog::print(const String &message) { (void)message; }
void SerialLog::printf(const char *format, ...) { (void)format; }

void setUp(void) {
    int res = sodium_init();
    (void)res;
}

void tearDown(void) {
}

void test_sha256_empty_string(void) {
    uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
    uint8_t dummy = 0;
    bool ok = FirmwareVerifier::computeSHA256(&dummy, 0, hash);
    TEST_ASSERT_TRUE(ok);

    // NIST official SHA-256 for empty string:
    // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    const uint8_t expected[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, hash, 32);
}

void test_sha256_known_string(void) {
    const char *msg = "abc";
    uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
    bool ok = FirmwareVerifier::computeSHA256((const uint8_t *)msg, strlen(msg), hash);
    TEST_ASSERT_TRUE(ok);

    // NIST official SHA-256 for "abc":
    // ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, hash, 32);
}

void test_sha256_null_parameters(void) {
    uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
    uint8_t dummy = 0;
    TEST_ASSERT_FALSE(FirmwareVerifier::computeSHA256(nullptr, 10, hash));
    TEST_ASSERT_FALSE(FirmwareVerifier::computeSHA256(&dummy, 1, nullptr));
}

void test_sha256_streaming_context(void) {
    FirmwareVerifier::SHA256Context ctx;
    TEST_ASSERT_TRUE(ctx.begin());
    TEST_ASSERT_TRUE(ctx.update((const uint8_t *)"a", 1));
    TEST_ASSERT_TRUE(ctx.update((const uint8_t *)"bc", 2));
    uint8_t streamingHash[FirmwareVerifier::SHA256_HASH_SIZE];
    TEST_ASSERT_TRUE(ctx.finish(streamingHash));

    uint8_t singleShotHash[FirmwareVerifier::SHA256_HASH_SIZE];
    TEST_ASSERT_TRUE(FirmwareVerifier::computeSHA256((const uint8_t *)"abc", 3, singleShotHash));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(singleShotHash, streamingHash, 32);
}

void test_parse_hex_hash_valid(void) {
    const char *hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
    TEST_ASSERT_TRUE(FirmwareVerifier::parseHexHash(hex, hash));
    TEST_ASSERT_EQUAL_HEX8(0xba, hash[0]);
    TEST_ASSERT_EQUAL_HEX8(0x78, hash[1]);
    TEST_ASSERT_EQUAL_HEX8(0xad, hash[31]);
}

void test_parse_hex_hash_invalid(void) {
    uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
    // Null pointer
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexHash(nullptr, hash));

    // Too short (63 chars)
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexHash(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015a", hash));

    // Too long (65 chars)
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexHash(
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad0", hash));

    // Invalid character 'z'
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexHash(
        "za7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", hash));
}

void test_parse_hex_hash_uppercase(void) {
    const char *hexUpper = "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD";
    uint8_t hash[FirmwareVerifier::SHA256_HASH_SIZE];
    TEST_ASSERT_TRUE(FirmwareVerifier::parseHexHash(hexUpper, hash));
    TEST_ASSERT_EQUAL_HEX8(0xba, hash[0]);
    TEST_ASSERT_EQUAL_HEX8(0x78, hash[1]);
    TEST_ASSERT_EQUAL_HEX8(0xad, hash[31]);
}

void test_parse_hex_null_output_pointers(void) {
    const char *validHash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexHash(validHash, nullptr));

    char hex128[129];
    memset(hex128, 'a', 128);
    hex128[128] = '\0';
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexSignature(hex128, nullptr));
}

void test_parse_hex_signature_valid_and_invalid(void) {
    uint8_t sig[FirmwareVerifier::ED25519_SIGNATURE_SIZE];

    // Null checks
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexSignature(nullptr, sig));

    // Valid 128-char hex string (64 zero bytes)
    char hex128[129];
    memset(hex128, 'a', 128);
    hex128[128] = '\0';
    TEST_ASSERT_TRUE(FirmwareVerifier::parseHexSignature(hex128, sig));
    TEST_ASSERT_EQUAL_HEX8(0xaa, sig[0]);
    TEST_ASSERT_EQUAL_HEX8(0xaa, sig[63]);

    // Invalid length (127 chars)
    hex128[127] = '\0';
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexSignature(hex128, sig));
}

void test_parse_hex_signature_invalid_chars(void) {
    uint8_t sig[FirmwareVerifier::ED25519_SIGNATURE_SIZE];
    char hex128[129];
    memset(hex128, 'a', 128);
    hex128[128] = '\0';

    // Replace a char with non-hex 'x'
    hex128[64] = 'x';
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexSignature(hex128, sig));

    // Replace with non-hex 'G'
    hex128[64] = 'G';
    TEST_ASSERT_FALSE(FirmwareVerifier::parseHexSignature(hex128, sig));
}

void test_to_hex_string_and_roundtrip(void) {
    const uint8_t testBytes[4] = {0xde, 0xad, 0xbe, 0xef};
    String hex = FirmwareVerifier::toHexString(testBytes, 4);
    TEST_ASSERT_EQUAL_STRING("deadbeef", hex.c_str());

    // 32-byte roundtrip
    uint8_t orig[32];
    for (int i = 0; i < 32; i++) orig[i] = (uint8_t)(i * 7 + 3);
    String hex32 = FirmwareVerifier::toHexString(orig, 32);
    TEST_ASSERT_EQUAL(64, hex32.length());

    uint8_t parsed[32];
    TEST_ASSERT_TRUE(FirmwareVerifier::parseHexHash(hex32.c_str(), parsed));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(orig, parsed, 32);
}

void test_ed25519_signature_verification(void) {
    uint8_t pk[crypto_sign_PUBLICKEYBYTES];
    uint8_t sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(pk, sk);

    // 32-byte message hash
    uint8_t hash[32];
    for (int i = 0; i < 32; i++) hash[i] = (uint8_t)(i + 1);

    // Sign the hash
    uint8_t sig[crypto_sign_BYTES];
    unsigned long long sigLen = 0;
    crypto_sign_detached(sig, &sigLen, hash, sizeof(hash), sk);
    TEST_ASSERT_EQUAL(64, sigLen);

    // Verify valid signature
    TEST_ASSERT_TRUE(FirmwareVerifier::verifySignature(hash, sig, pk));

    // Null pointer checks
    TEST_ASSERT_FALSE(FirmwareVerifier::verifySignature(nullptr, sig, pk));
    TEST_ASSERT_FALSE(FirmwareVerifier::verifySignature(hash, nullptr, pk));
    TEST_ASSERT_FALSE(FirmwareVerifier::verifySignature(hash, sig, nullptr));

    // Corrupted signature (flip 1 byte)
    uint8_t badSig[64];
    memcpy(badSig, sig, 64);
    badSig[0] ^= 0xFF;
    TEST_ASSERT_FALSE(FirmwareVerifier::verifySignature(hash, badSig, pk));

    // Corrupted hash (flip 1 byte)
    uint8_t badHash[32];
    memcpy(badHash, hash, 32);
    badHash[10] ^= 0x01;
    TEST_ASSERT_FALSE(FirmwareVerifier::verifySignature(badHash, sig, pk));
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_sha256_empty_string);
    RUN_TEST(test_sha256_known_string);
    RUN_TEST(test_sha256_null_parameters);
    RUN_TEST(test_sha256_streaming_context);
    RUN_TEST(test_parse_hex_hash_valid);
    RUN_TEST(test_parse_hex_hash_uppercase);
    RUN_TEST(test_parse_hex_hash_invalid);
    RUN_TEST(test_parse_hex_null_output_pointers);
    RUN_TEST(test_parse_hex_signature_valid_and_invalid);
    RUN_TEST(test_parse_hex_signature_invalid_chars);
    RUN_TEST(test_to_hex_string_and_roundtrip);
    RUN_TEST(test_ed25519_signature_verification);
    UNITY_END();
}

void loop() {
    // Empty loop
}
