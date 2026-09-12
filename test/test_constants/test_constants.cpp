#include <unity.h>
#include <Arduino.h>
#include "Constants.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_hardware_pin_definitions(void) {
    TEST_ASSERT_EQUAL(5, SNOOZE_BUTTON_PIN);
    TEST_ASSERT_EQUAL(0, BOOT_BUTTON_PIN);
    TEST_ASSERT_EQUAL(2, RTC_INT_PIN);
    TEST_ASSERT_EQUAL(4, BUZZER_PIN);
}

void test_alarm_progression_timings(void) {
    // Progressive buzzer ramping constants
    TEST_ASSERT_EQUAL(10000, STAGE1_DURATION_MS);
    TEST_ASSERT_EQUAL(20000, STAGE2_DURATION_MS);
    TEST_ASSERT_EQUAL(1800, ALARM_AUTO_OFF_SECONDS); // 30 minutes

    // Slow beeping cadence
    TEST_ASSERT_EQUAL(200, SLOW_BEEP_ON_MS);
    TEST_ASSERT_EQUAL(800, SLOW_BEEP_OFF_MS);

    // Fast beeping cadence
    TEST_ASSERT_EQUAL(150, FAST_BEEP_ON_MS);
    TEST_ASSERT_EQUAL(150, FAST_BEEP_OFF_MS);

    // Hold durations
    TEST_ASSERT_EQUAL(3000, SNOOZE_DISMISS_HOLD_TIME);
    TEST_ASSERT_EQUAL(5000, ALARM_RESUME_DELAY_MS);
}

void test_display_sprite_geometry(void) {
    TEST_ASSERT_EQUAL(10, MARGIN);
    TEST_ASSERT_EQUAL(343, CLOCK_SPRITE_WIDTH);
    TEST_ASSERT_EQUAL(106, CLOCK_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(45, TOD_SPRITE_WIDTH);
    TEST_ASSERT_EQUAL(34, TOD_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(70, SECONDS_SPRITE_WIDTH);
    TEST_ASSERT_EQUAL(50, SECONDS_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(50, DAY_OF_WEEK_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(50, DATE_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(50, TEMP_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(50, HUMIDITY_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(50, ALARM_SPRITE_HEIGHT);
    TEST_ASSERT_EQUAL(6, ALARM_PROGRESS_BAR_HEIGHT);
    TEST_ASSERT_EQUAL(462, ALARM_ICON_X);
    TEST_ASSERT_EQUAL(5, ALARM_ICON_Y);
}

void test_brightness_limits(void) {
    TEST_ASSERT_EQUAL(5, BRIGHTNESS_MIN);
    TEST_ASSERT_EQUAL(255, BRIGHTNESS_MAX);
    TEST_ASSERT_TRUE(BRIGHTNESS_MIN < BRIGHTNESS_MAX);
}

void test_safe_mode_invariants(void) {
    TEST_ASSERT_EQUAL(10, SAFE_MODE_BOOT_THRESHOLD);
    TEST_ASSERT_EQUAL_STRING("Clock-SafeMode", SAFE_MODE_AP_SSID);
    TEST_ASSERT_EQUAL_STRING("safemode", SAFE_MODE_NVS_NAMESPACE);
    TEST_ASSERT_EQUAL_STRING("bootcount", SAFE_MODE_NVS_KEY);
    TEST_ASSERT_EQUAL(15000, SAFE_MODE_WIFI_TIMEOUT);
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_hardware_pin_definitions);
    RUN_TEST(test_alarm_progression_timings);
    RUN_TEST(test_display_sprite_geometry);
    RUN_TEST(test_brightness_limits);
    RUN_TEST(test_safe_mode_invariants);
    UNITY_END();
}

void loop() {
    // Empty loop
}
