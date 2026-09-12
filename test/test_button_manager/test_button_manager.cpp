#include <unity.h>
#include "ButtonManager.h"
#include "../../src/ButtonManager.cpp"

void setUp(void) {
}

void tearDown(void) {
}

void test_button_manager_initialization() {
    ButtonManager btn(5);
    // Should not crash
    btn.begin();
    
    // Initially no press available
    TEST_ASSERT_FALSE(btn.newPressAvailable());
    TEST_ASSERT_EQUAL(0, btn.getPressDuration());
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_button_manager_initialization);
    UNITY_END();
}

void loop() {}
