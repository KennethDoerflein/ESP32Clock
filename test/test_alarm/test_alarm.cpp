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

void test_alarm_dst_spring_forward_skipped_hour(void) {
    Alarm alarm;
    alarm.setHour(2);
    alarm.setMinute(30);
    alarm.setDays(DAY_SUN);
    alarm.setEnabled(true);

    // Simulated checkSkipped created during spring forward skipped hour (2:30 AM on Sun Mar 8, 2026)
    DateTime checkSkipped(2026, 3, 8, 2, 30, 0);
    TEST_ASSERT_EQUAL(2, checkSkipped.hour());
    TEST_ASSERT_EQUAL(30, checkSkipped.minute());
    TEST_ASSERT_TRUE(alarm.shouldRing(checkSkipped));

    // A minute that doesn't match should not ring
    DateTime checkSkippedOther(2026, 3, 8, 2, 15, 0);
    TEST_ASSERT_FALSE(alarm.shouldRing(checkSkippedOther));
}

void test_alarm_dst_fall_back_dismiss_no_duplicate(void) {
    Alarm alarm;
    alarm.setHour(1);
    alarm.setMinute(30);
    alarm.setDays(DAY_SUN | DAY_MON);
    alarm.setEnabled(true);

    // Ring at first 1:30 AM on fall back day (Sun Nov 1, 2026)
    DateTime firstRing(2026, 11, 1, 1, 30, 0);
    TEST_ASSERT_TRUE(alarm.shouldRing(firstRing));

    // User dismisses the alarm
    alarm.dismiss(firstRing);
    TEST_ASSERT_TRUE(alarm.isEnabled()); // Still enabled since repeating

    // At the second 1:30 AM on the same day after fall back, should not ring again
    DateTime secondRing(2026, 11, 1, 1, 30, 0);
    TEST_ASSERT_FALSE(alarm.shouldRing(secondRing));

    // Next day (Mon Nov 2, 2026) at 1:30 AM, should ring normally
    DateTime nextDayRing(2026, 11, 2, 1, 30, 0);
    TEST_ASSERT_TRUE(alarm.shouldRing(nextDayRing));
}

void test_alarm_biweekly_parity_across_dst(void) {
    // 2026 US Spring Forward: Sunday March 8, 2026
    DateTime sat(2026, 3, 7, 23, 0, 0);
    DateTime sunDst(2026, 3, 8, 3, 30, 0);
    DateTime mon(2026, 3, 9, 8, 0, 0);

    // Sat and Sun belong to the same week; Mon begins the next week
    TEST_ASSERT_EQUAL(Alarm::isOddWeek(sat), Alarm::isOddWeek(sunDst));
    TEST_ASSERT_NOT_EQUAL(Alarm::isOddWeek(sunDst), Alarm::isOddWeek(mon));
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
    RUN_TEST(test_alarm_dst_spring_forward_skipped_hour);
    RUN_TEST(test_alarm_dst_fall_back_dismiss_no_duplicate);
    RUN_TEST(test_alarm_biweekly_parity_across_dst);
    UNITY_END();
}

void loop() {
    // Empty loop
}
