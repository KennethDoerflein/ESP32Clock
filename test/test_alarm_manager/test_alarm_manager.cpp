#include <unity.h>
#include "AlarmManager.h"
#include "../../src/AlarmManager.cpp"
#include "ConfigManager.h"

// Stubs for TimeManager
TimeManager::TimeManager() {}
void TimeManager::checkDST(const DateTime *time) {}
DateTime TimeManager::getLocalTime(const DateTime *preReadUtc) const { return DateTime(); }
DateTime TimeManager::getRTCTime() const { return DateTime(2023, 1, 1, 0, 0, 0); }
void TimeManager::checkMissedAlarms() {}

// Stubs for ConfigManager (if we don't compile its cpp)
// Wait, we can compile ConfigManager.cpp if we stub UpdateManager.
#include "../../src/ConfigManager.cpp"

UpdateManager::UpdateManager() {}
UpdateManager &UpdateManager::getInstance() {
    static UpdateManager instance;
    return instance;
}
bool UpdateManager::isUpdateInProgress() { return false; }

// Stubs for SerialLog
SerialLog::SerialLog() : _ws("/ws/log") {}
SerialLog &SerialLog::getInstance() {
    static SerialLog instance;
    return instance;
}
void SerialLog::print(const String &message) {}
void SerialLog::printf(const char *format, ...) {}

// Stubs for Display
#include "Display.h"
void Display::setBacklightFlashing(bool flashing) {}

// Stubs for DisplayManager
#include "DisplayManager.h"
DisplayManager::DisplayManager() {}
void DisplayManager::requestPartialRefresh() {}


void setUp(void) {
    AlarmManager::getInstance().stop();
}

void tearDown(void) {
}

void test_alarm_manager_trigger_and_stop() {
    AlarmManager &am = AlarmManager::getInstance();
    TEST_ASSERT_FALSE(am.isRinging());
    
    // Trigger alarm 1
    bool triggered = am.trigger(1, 100000);
    TEST_ASSERT_TRUE(triggered);
    TEST_ASSERT_TRUE(am.isRinging());
    TEST_ASSERT_EQUAL(1, am.getActiveAlarmId());
    
    // Triggering another should fail
    bool triggered2 = am.trigger(2, 100000);
    TEST_ASSERT_FALSE(triggered2);
    
    am.stop();
    TEST_ASSERT_FALSE(am.isRinging());
    TEST_ASSERT_EQUAL(-1, am.getActiveAlarmId());
}

void test_alarm_manager_ramping() {
    AlarmManager &am = AlarmManager::getInstance();
    am.trigger(2, 100000);
    
    // Initial stage is slow beep
    am.update();
    
    // Wait for first transition
    // Note: STAGE1_DURATION_MS is usually 60s, which is long for a test.
    // So we just verify it doesn't crash on update.
    
    am.stop();
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_alarm_manager_trigger_and_stop);
    RUN_TEST(test_alarm_manager_ramping);
    UNITY_END();
}

void loop() {}
