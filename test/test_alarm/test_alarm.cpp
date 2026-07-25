#include <unity.h>
#include "Alarm.h"
#include <RTClib.h>

void setUp(void) {
    // Set up code runs before each test
}

void tearDown(void) {
    // Tear down code runs after each test
}

void test_alarm_default_constructor(void) {
    Alarm alarm;
    TEST_ASSERT_EQUAL(6, alarm.getHour());
    TEST_ASSERT_EQUAL(0, alarm.getMinute());
    TEST_ASSERT_FALSE(alarm.isEnabled());
    TEST_ASSERT_FALSE(alarm.isSnoozed());
}

void test_alarm_one_time_dismiss(void) {
    Alarm alarm;
    alarm.setHour(7);
    alarm.setMinute(30);
    alarm.setEnabled(true);
    
    DateTime ringTime(2023, 10, 24, 7, 30, 0); // Some day at 7:30
    TEST_ASSERT_TRUE(alarm.shouldRing(ringTime));
    
    alarm.dismiss(ringTime);
    TEST_ASSERT_FALSE(alarm.isEnabled()); // One-time alarm should disable itself
}

void test_alarm_repeating_dismiss(void) {
    Alarm alarm;
    alarm.setHour(8);
    alarm.setMinute(0);
    alarm.setDays(DAY_MON | DAY_WED);
    alarm.setEnabled(true);
    
    DateTime monday(2023, 10, 23, 8, 0, 0); // Monday 8:00
    TEST_ASSERT_TRUE(alarm.shouldRing(monday));
    
    alarm.dismiss(monday);
    TEST_ASSERT_TRUE(alarm.isEnabled()); // Repeating alarm should remain enabled
    TEST_ASSERT_FALSE(alarm.shouldRing(monday)); // But shouldn't ring again today
    
    DateTime nextMonday(2023, 10, 30, 8, 0, 0); // Next Monday
    TEST_ASSERT_TRUE(alarm.shouldRing(nextMonday)); // Should ring next week
}

void test_alarm_biweekly_parity(void) {
    Alarm alarm;
    alarm.setHour(6);
    alarm.setMinute(0);
    alarm.setDays(DAY_TUE);
    alarm.setBiweekly(true);
    alarm.setBiweeklyOddWeek(true);
    alarm.setEnabled(true);
    
    DateTime tue1(2024, 1, 2, 6, 0, 0); // Jan 2, 2024 is Tue
    bool isOdd1 = Alarm::isOddWeek(tue1);
    
    DateTime tue2(2024, 1, 9, 6, 0, 0); // Next Tue
    bool isOdd2 = Alarm::isOddWeek(tue2);
    
    TEST_ASSERT_NOT_EQUAL(isOdd1, isOdd2); // Parity must flip every week
    
    if (isOdd1) {
        TEST_ASSERT_TRUE(alarm.shouldRing(tue1));
        TEST_ASSERT_FALSE(alarm.shouldRing(tue2));
    } else {
        TEST_ASSERT_FALSE(alarm.shouldRing(tue1));
        TEST_ASSERT_TRUE(alarm.shouldRing(tue2));
    }
}

int main(int argc, char **argv) {
    // This is required for Arduino/PlatformIO on-target testing
}

void setup() {
    delay(2000); // Give some time for the serial monitor to connect
    UNITY_BEGIN();
    RUN_TEST(test_alarm_default_constructor);
    RUN_TEST(test_alarm_one_time_dismiss);
    RUN_TEST(test_alarm_repeating_dismiss);
    RUN_TEST(test_alarm_biweekly_parity);
    UNITY_END();
}

void loop() {
    // Empty loop
}
