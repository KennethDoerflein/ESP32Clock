#include <unity.h>
#include "ConfigManager.h"
#include "../../src/ConfigManager.cpp"
#include <vector>

// Minimal SerialLog stubs to isolate test
SerialLog::SerialLog() : _ws("/ws/log") {}
SerialLog &SerialLog::getInstance() {
    static SerialLog instance;
    return instance;
}
void SerialLog::print(const String &message) { (void)message; }
void SerialLog::printf(const char *format, ...) { (void)format; }

// Minimal UpdateManager stub
UpdateManager &UpdateManager::getInstance() {
    static UpdateManager instance;
    return instance;
}
bool UpdateManager::isUpdateInProgress() {
    return false;
}

// Minimal TimeManager stub
#include "TimeManager.h"
TimeManager::TimeManager() {}
void TimeManager::checkDST(const DateTime *time) {
    (void)time;
}

void setUp(void) {
}

void tearDown(void) {
}

void test_config_default_constants(void) {
    TEST_ASSERT_EQUAL(5, LEGACY_ALARMS_COUNT);
    TEST_ASSERT_EQUAL(2, DEFAULT_ALARMS_COUNT);
    TEST_ASSERT_EQUAL(20, MAX_ALLOWED_ALARMS);

    // Default Colors
    TEST_ASSERT_EQUAL_STRING("#000000", DEFAULT_BACKGROUND_COLOR);
    TEST_ASSERT_EQUAL_STRING("#079ae4", DEFAULT_TIME_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FFFFFF", DEFAULT_TOD_COLOR);
    TEST_ASSERT_EQUAL_STRING("#02e3ab", DEFAULT_SECONDS_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FFFFFF", DEFAULT_DAY_OF_WEEK_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FFFFFF", DEFAULT_DATE_COLOR);
    TEST_ASSERT_EQUAL_STRING("#02e3ab", DEFAULT_TEMP_COLOR);
    TEST_ASSERT_EQUAL_STRING("#079ae4", DEFAULT_HUMIDITY_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FFFF00", DEFAULT_ALARM_ICON_COLOR);
    TEST_ASSERT_EQUAL_STRING("#0000FF", DEFAULT_SNOOZE_ICON_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FF0000", DEFAULT_ALARM_TEXT_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FF0000", DEFAULT_ERROR_TEXT_COLOR);
    TEST_ASSERT_EQUAL_STRING("#02e3ab", DEFAULT_WEATHER_TEMP_COLOR);
    TEST_ASSERT_EQUAL_STRING("#FFFFFF", DEFAULT_WEATHER_FORECAST_COLOR);

    // Default Brightness and Times
    TEST_ASSERT_TRUE(DEFAULT_AUTO_BRIGHTNESS);
    TEST_ASSERT_EQUAL(128, DEFAULT_BRIGHTNESS);
    TEST_ASSERT_EQUAL(7, DEFAULT_AUTO_BRIGHTNESS_START_HOUR);
    TEST_ASSERT_EQUAL(21, DEFAULT_AUTO_BRIGHTNESS_END_HOUR);
    TEST_ASSERT_EQUAL(255, DEFAULT_DAY_BRIGHTNESS);
    TEST_ASSERT_EQUAL(10, DEFAULT_NIGHT_BRIGHTNESS);

    // Default durations
    TEST_ASSERT_EQUAL(9, DEFAULT_SNOOZE_DURATION);
    TEST_ASSERT_EQUAL(3, DEFAULT_DISMISS_DURATION);

    // Default thermal compensation
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.50f, DEFAULT_TEMP_COMPENSATION_FACTOR);
    TEST_ASSERT_TRUE(DEFAULT_AUTO_TEMP_CALIBRATION);
    TEST_ASSERT_TRUE(DEFAULT_TEMP_CORRECTION_ENABLED);
}

void test_config_default_pages(void) {
    // 0: Clock, 1: Weather, 3: Weather+Clock, 2: Info
    TEST_ASSERT_EQUAL(0, DEFAULT_ENABLED_PAGES[0]);
    TEST_ASSERT_EQUAL(1, DEFAULT_ENABLED_PAGES[1]);
    TEST_ASSERT_EQUAL(3, DEFAULT_ENABLED_PAGES[2]);
    TEST_ASSERT_EQUAL(2, DEFAULT_ENABLED_PAGES[3]);
    TEST_ASSERT_EQUAL(0, DEFAULT_DEFAULT_PAGE);
}

void test_config_manager_alarms_and_summary(void) {
    ConfigManager &config = ConfigManager::getInstance();

    std::vector<Alarm> testAlarms(3);
    testAlarms[0].setId(1);
    testAlarms[0].setEnabled(false);
    testAlarms[0].setHour(7);
    testAlarms[0].setMinute(0);

    testAlarms[1].setId(2);
    testAlarms[1].setEnabled(false);
    testAlarms[1].setHour(8);
    testAlarms[1].setMinute(30);

    testAlarms[2].setId(3);
    testAlarms[2].setEnabled(false);
    testAlarms[2].setHour(9);
    testAlarms[2].setMinute(15);

    config.replaceAlarms(testAlarms);
    TEST_ASSERT_EQUAL(3, config.getNumAlarms());

    // When none enabled or snoozed
    bool anyEnabled = true;
    bool anySnoozed = true;
    config.getAlarmSummary(anyEnabled, anySnoozed);
    TEST_ASSERT_FALSE(anyEnabled);
    TEST_ASSERT_FALSE(anySnoozed);
    TEST_ASSERT_FALSE(config.isAnyAlarmSnoozed());
    TEST_ASSERT_EQUAL(0, config.getFirstSnoozedUntil());

    // Enable one alarm
    testAlarms[0].setEnabled(true);
    config.replaceAlarms(testAlarms);
    config.getAlarmSummary(anyEnabled, anySnoozed);
    TEST_ASSERT_TRUE(anyEnabled);
    TEST_ASSERT_FALSE(anySnoozed);

    // Snooze the enabled alarm
    testAlarms[0].setSnoozeState(true, 1700005000);
    config.replaceAlarms(testAlarms);
    config.getAlarmSummary(anyEnabled, anySnoozed);
    TEST_ASSERT_TRUE(anyEnabled);
    TEST_ASSERT_TRUE(anySnoozed);
    TEST_ASSERT_TRUE(config.isAnyAlarmSnoozed());
    TEST_ASSERT_EQUAL(1700005000, config.getFirstSnoozedUntil());
}

void test_config_manager_alarm_access_by_index_and_id(void) {
    ConfigManager &config = ConfigManager::getInstance();

    std::vector<Alarm> testAlarms(2);
    testAlarms[0].setId(10);
    testAlarms[0].setHour(6);
    testAlarms[0].setMinute(45);

    testAlarms[1].setId(20);
    testAlarms[1].setHour(7);
    testAlarms[1].setMinute(15);

    config.replaceAlarms(testAlarms);

    // Access by index
    Alarm byIdx = config.getAlarmByIndex(0);
    TEST_ASSERT_EQUAL(10, byIdx.getId());
    TEST_ASSERT_EQUAL(6, byIdx.getHour());

    // Access by ID
    Alarm byId = config.getAlarmById(20);
    TEST_ASSERT_EQUAL(20, byId.getId());
    TEST_ASSERT_EQUAL(7, byId.getHour());

    // Non-existent ID returns ID 255
    Alarm notFound = config.getAlarmById(99);
    TEST_ASSERT_EQUAL(255, notFound.getId());

    // Update by ID
    byId.setMinute(30);
    config.setAlarmById(20, byId);
    TEST_ASSERT_EQUAL(30, config.getAlarmById(20).getMinute());
}

void test_config_manager_settings_getters_setters(void) {
    ConfigManager &config = ConfigManager::getInstance();

    // Brightness
    config.setBrightness(180);
    TEST_ASSERT_EQUAL(180, config.getBrightness());

    // WiFi SSID
    config.setWifiSSID("TestNet");
    TEST_ASSERT_EQUAL_STRING("TestNet", config.getWifiSSID().c_str());

    // 24 Hour format
    config.set24HourFormat(true);
    TEST_ASSERT_TRUE(config.is24HourFormat());
    config.set24HourFormat(false);
    TEST_ASSERT_FALSE(config.is24HourFormat());

    // Celsius
    config.setCelsius(true);
    TEST_ASSERT_TRUE(config.isCelsius());
    config.setCelsius(false);
    TEST_ASSERT_FALSE(config.isCelsius());

    // Timezone
    config.setTimezone("UTC0");
    TEST_ASSERT_EQUAL_STRING("UTC0", config.getTimezone().c_str());

    // Reset to defaults
    config.resetGeneralSettingsToDefaults();
    TEST_ASSERT_EQUAL(DEFAULT_BRIGHTNESS, config.getBrightness());
    TEST_ASSERT_EQUAL(DEFAULT_USE_24_HOUR_FORMAT, config.is24HourFormat());
    TEST_ASSERT_EQUAL(DEFAULT_USE_CELSIUS, config.isCelsius());
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_config_default_constants);
    RUN_TEST(test_config_default_pages);
    RUN_TEST(test_config_manager_alarms_and_summary);
    RUN_TEST(test_config_manager_alarm_access_by_index_and_id);
    RUN_TEST(test_config_manager_settings_getters_setters);
    UNITY_END();
}

void loop() {
    // Empty loop
}
