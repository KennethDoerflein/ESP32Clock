#include <unity.h>
#include "WiFiManager.h"
#include <Arduino.h>

// Stubs for ConfigManager
#include "ConfigManager.h"
bool ConfigManager::save() { return true; }
String ConfigManager::getHostname() const { return "clock"; }
void ConfigManager::setHostname(const String &h) {}


// Stubs for Display
#include "Display.h"
void Display::drawMultiLineStatusMessage(const char *line1, const char *line2) {}
void Display::drawStatusMessage(const char *message) {}

// Stubs for DisplayManager
#include "DisplayManager.h"
DisplayManager::DisplayManager() {}
void DisplayManager::requestFullRefresh() {}

// Stubs for SerialLog
#include "SerialLog.h"
SerialLog::SerialLog() : _ws("/ws/log") {}
SerialLog &SerialLog::getInstance() {
    static SerialLog instance;
    return instance;
}
void SerialLog::print(const String &message) {}
void SerialLog::printf(const char *format, ...) {}

// Stubs for TimeManager
#include "TimeManager.h"
TimeManager::TimeManager() {}

#include "../../src/WiFiManager.cpp"


void setUp(void) {
}

void tearDown(void) {
}

void test_wifi_manager_initial_state() {
    WiFiManager &wm = WiFiManager::getInstance();
    TEST_ASSERT_EQUAL(WiFiManager::TEST_IDLE, wm.getConnectionTestStatus());
    TEST_ASSERT_FALSE(wm.isPendingReboot());
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_wifi_manager_initial_state);
    UNITY_END();
}

void loop() {}
