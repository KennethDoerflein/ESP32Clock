#include <unity.h>
#include "Utils.h"
#include "../../src/Utils.cpp"

void setUp(void) {
    // Set up code runs before each test
}

void tearDown(void) {
    // Tear down code runs after each test
}

void test_hex_to_rgb565_primary_colors(void) {
    // Black (#000000) -> 0x0000
    TEST_ASSERT_EQUAL_HEX16(0x0000, hexToRGB565("#000000"));

    // White (#FFFFFF) -> 0xFFFF
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, hexToRGB565("#FFFFFF"));

    // Pure Red (#FF0000): r=255 -> 31<<11 = 0xF800, g=0, b=0
    TEST_ASSERT_EQUAL_HEX16(0xF800, hexToRGB565("#FF0000"));

    // Pure Green (#00FF00): r=0, g=255 -> 63<<5 = 0x07E0, b=0
    TEST_ASSERT_EQUAL_HEX16(0x07E0, hexToRGB565("#00FF00"));

    // Pure Blue (#0000FF): r=0, g=0, b=255 -> 31 = 0x001F
    TEST_ASSERT_EQUAL_HEX16(0x001F, hexToRGB565("#0000FF"));
}

void test_hex_to_rgb565_theme_colors(void) {
    // Yellow (#FFFF00) -> Red + Green = 0xFFE0
    TEST_ASSERT_EQUAL_HEX16(0xFFE0, hexToRGB565("#FFFF00"));

    // Cyan (#00FFFF) -> Green + Blue = 0x07FF
    TEST_ASSERT_EQUAL_HEX16(0x07FF, hexToRGB565("#00FFFF"));

    // Magenta (#FF00FF) -> Red + Blue = 0xF81F
    TEST_ASSERT_EQUAL_HEX16(0xF81F, hexToRGB565("#FF00FF"));

    // App default TIME color: #079ae4
    // R: 0x07 (7/8 = 0)
    // G: 0x9a (154/4 = 38)
    // B: 0xe4 (228/8 = 28)
    // Expected: (0 << 11) | (38 << 5) | 28 = 0x04DC
    uint16_t timeColor = hexToRGB565("#079ae4");
    TEST_ASSERT_EQUAL_HEX16(((7 / 8) << 11) | ((0x9A / 4) << 5) | (0xE4 / 8), timeColor);
}

void test_hex_to_rgb565_edge_cases(void) {
    // Short string (< 2 characters) returns 0
    TEST_ASSERT_EQUAL_HEX16(0, hexToRGB565(""));
    TEST_ASSERT_EQUAL_HEX16(0, hexToRGB565("#"));
    TEST_ASSERT_EQUAL_HEX16(0, hexToRGB565("A"));

    // Case insensitivity: lowercase vs uppercase
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, hexToRGB565("#ffffff"));
    TEST_ASSERT_EQUAL_HEX16(0xF800, hexToRGB565("#ff0000"));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, hexToRGB565("#00ff00"));
}

void test_is_valid_hostname_valid_cases(void) {
    // Standard alphanumeric with hyphen
    TEST_ASSERT_TRUE(isValidHostname("esp32-clock"));
    TEST_ASSERT_TRUE(isValidHostname("clock"));
    TEST_ASSERT_TRUE(isValidHostname("smart-clock-2024"));
    TEST_ASSERT_TRUE(isValidHostname("a")); // Min length 1
    TEST_ASSERT_TRUE(isValidHostname("A-B-C-1-2-3"));

    // Maximum 63 characters
    String maxLenHost = "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0";
    TEST_ASSERT_EQUAL(63, maxLenHost.length());
    TEST_ASSERT_TRUE(isValidHostname(maxLenHost));
}

void test_is_valid_hostname_invalid_cases(void) {
    // Empty string (length 0)
    TEST_ASSERT_FALSE(isValidHostname(""));

    // Exceeds 63 characters
    String tooLong = "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz01";
    TEST_ASSERT_EQUAL(64, tooLong.length());
    TEST_ASSERT_FALSE(isValidHostname(tooLong));

    // Leading hyphen
    TEST_ASSERT_FALSE(isValidHostname("-esp32clock"));

    // Trailing hyphen
    TEST_ASSERT_FALSE(isValidHostname("esp32clock-"));

    // Invalid characters
    TEST_ASSERT_FALSE(isValidHostname("esp32.clock"));     // Dot not allowed in single label hostname
    TEST_ASSERT_FALSE(isValidHostname("esp32_clock"));     // Underscore not allowed in RFC 1123 hostname
    TEST_ASSERT_FALSE(isValidHostname("esp32 clock"));     // Space not allowed
    TEST_ASSERT_FALSE(isValidHostname("clock@home"));      // @ not allowed
    TEST_ASSERT_FALSE(isValidHostname("clock!123"));       // ! not allowed
    TEST_ASSERT_FALSE(isValidHostname("clock#1"));         // # not allowed
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_hex_to_rgb565_primary_colors);
    RUN_TEST(test_hex_to_rgb565_theme_colors);
    RUN_TEST(test_hex_to_rgb565_edge_cases);
    RUN_TEST(test_is_valid_hostname_valid_cases);
    RUN_TEST(test_is_valid_hostname_invalid_cases);
    UNITY_END();
}

void loop() {
    // Empty loop
}
