#include <unity.h>
#include "UpdateManager.h"
#include "../../src/UpdateManager.cpp"


void setUp(void) {
}

void tearDown(void) {
}

void test_update_manager_instance() {
    UpdateManager &um = UpdateManager::getInstance();
    TEST_ASSERT_FALSE(um.isUpdateInProgress());
    TEST_ASSERT_EQUAL_STRING("", um.getLastError().c_str());
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_update_manager_instance);
    UNITY_END();
}

void loop() {}
