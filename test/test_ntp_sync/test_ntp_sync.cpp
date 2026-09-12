#include <unity.h>
#include "NtpSync.h"
#include <Arduino.h>

// Stubs for ConfigManager
#include "ConfigManager.h"
String ConfigManager::getTimezone() const { return "UTC"; }
void ConfigManager::load() {}

// Stubs for TimeManager
#include "TimeManager.h"
TimeManager::TimeManager() {}
DateTime TimeManager::getRTCTime() const { return DateTime(2023, 1, 1, 0, 0, 0); }
void TimeManager::adjustRTC(const DateTime &time) {}
void TimeManager::checkDST(const DateTime *time) {}
void TimeManager::checkMissedAlarmsWindow(time_t start, time_t end) {}
void TimeManager::setNextAlarms() {}

// Stubs for SerialLog
#include "SerialLog.h"
SerialLog::SerialLog() : _ws("/ws/log") {}
SerialLog &SerialLog::getInstance() {
    static SerialLog instance;
    return instance;
}
void SerialLog::print(const String &message) {}
void SerialLog::printf(const char *format, ...) {}

#include "../../src/NtpSync.cpp"


void setUp(void) {
    resetNtpSync();
}

void tearDown(void) {
}

void test_ntp_sync_initial_state() {
    TEST_ASSERT_EQUAL(NTP_SYNC_IDLE, updateNtpSync());
    TEST_ASSERT_FALSE(isNtpSyncInProgress());
}

void test_ntp_sync_start_and_reset() {
    startNtpSync();
    TEST_ASSERT_TRUE(isNtpSyncInProgress());
    TEST_ASSERT_EQUAL(NTP_SYNC_IN_PROGRESS, updateNtpSync());
    
    resetNtpSync();
    TEST_ASSERT_FALSE(isNtpSyncInProgress());
    TEST_ASSERT_EQUAL(NTP_SYNC_IDLE, updateNtpSync());
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_ntp_sync_initial_state);
    RUN_TEST(test_ntp_sync_start_and_reset);
    UNITY_END();
}

void loop() {}
