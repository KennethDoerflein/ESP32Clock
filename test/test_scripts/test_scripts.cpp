#include <unity.h>
#include <Arduino.h>
#include <cstring>
#if __has_include("version.h")
#include "version.h"
#else
#include "version.h.default"
#endif

void setUp(void) {
}

void tearDown(void) {
}

void test_firmware_version_defined(void) {
#ifdef FIRMWARE_VERSION
    TEST_ASSERT_TRUE(strlen(FIRMWARE_VERSION) > 0);
#else
    TEST_FAIL_MESSAGE("FIRMWARE_VERSION is not defined");
#endif
}

void test_firmware_version_format(void) {
#ifdef FIRMWARE_VERSION
    const char *ver = FIRMWARE_VERSION;
    // Version must start with 'v' (tag e.g. v1.0.0) or 'dev-' (local build)
    bool validPrefix = (ver[0] == 'v') || (strncmp(ver, "dev-", 4) == 0);
    TEST_ASSERT_TRUE(validPrefix);
#endif
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_firmware_version_defined);
    RUN_TEST(test_firmware_version_format);
    UNITY_END();
}

void loop() {
    // Empty loop
}
