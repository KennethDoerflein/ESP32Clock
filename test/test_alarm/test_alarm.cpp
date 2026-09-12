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
    TEST_ASSERT_EQUAL(0, alarm.getDays());
    TEST_ASSERT_FALSE(alarm.isBiweekly());
    TEST_ASSERT_FALSE(alarm.isBiweeklyOddWeek());
    TEST_ASSERT_EQUAL(0, alarm.getSnoozeUntil());
    TEST_ASSERT_EQUAL(0, alarm.getLastDismissedDayEpoch());
}

void test_alarm_set_hour_clamping(void) {
    Alarm alarm;
    alarm.setHour(0);
    TEST_ASSERT_EQUAL(0, alarm.getHour());
    alarm.setHour(12);
    TEST_ASSERT_EQUAL(12, alarm.getHour());
    alarm.setHour(23);
    TEST_ASSERT_EQUAL(23, alarm.getHour());
    alarm.setHour(24);
    TEST_ASSERT_EQUAL(23, alarm.getHour()); // Clamped to 23
    alarm.setHour(99);
    TEST_ASSERT_EQUAL(23, alarm.getHour()); // Clamped to 23
}

void test_alarm_set_minute_clamping(void) {
    Alarm alarm;
    alarm.setMinute(0);
    TEST_ASSERT_EQUAL(0, alarm.getMinute());
    alarm.setMinute(30);
    TEST_ASSERT_EQUAL(30, alarm.getMinute());
    alarm.setMinute(59);
    TEST_ASSERT_EQUAL(59, alarm.getMinute());
    alarm.setMinute(60);
    TEST_ASSERT_EQUAL(59, alarm.getMinute()); // Clamped to 59
    alarm.setMinute(100);
    TEST_ASSERT_EQUAL(59, alarm.getMinute()); // Clamped to 59
}

void test_alarm_id_and_enable(void) {
    Alarm alarm;
    alarm.setId(42);
    TEST_ASSERT_EQUAL(42, alarm.getId());
    TEST_ASSERT_FALSE(alarm.isEnabled());
    alarm.setEnabled(true);
    TEST_ASSERT_TRUE(alarm.isEnabled());
    alarm.setEnabled(false);
    TEST_ASSERT_FALSE(alarm.isEnabled());
}

void test_alarm_disabling_clears_snooze(void) {
    Alarm alarm;
    alarm.setEnabled(true);
    alarm.setSnoozeState(true, 1700000000);
    TEST_ASSERT_TRUE(alarm.isSnoozed());
    TEST_ASSERT_EQUAL(1700000000, alarm.getSnoozeUntil());

    // Disabling must clear snooze
    alarm.setEnabled(false);
    TEST_ASSERT_FALSE(alarm.isSnoozed());
    TEST_ASSERT_EQUAL(0, alarm.getSnoozeUntil());
}

void test_alarm_update_snooze(void) {
    Alarm alarm;
    alarm.setSnoozeState(true, 1000);

    // When current epoch is before snooze expiration
    TEST_ASSERT_FALSE(alarm.updateSnooze(999));
    TEST_ASSERT_TRUE(alarm.isSnoozed());

    // When current epoch equals snooze expiration
    TEST_ASSERT_FALSE(alarm.updateSnooze(1000));
    TEST_ASSERT_TRUE(alarm.isSnoozed());

    // When current epoch exceeds snooze expiration
    TEST_ASSERT_TRUE(alarm.updateSnooze(1001));
    TEST_ASSERT_FALSE(alarm.isSnoozed());
    TEST_ASSERT_EQUAL(0, alarm.getSnoozeUntil());

    // Calling when already not snoozed returns false
    TEST_ASSERT_FALSE(alarm.updateSnooze(1002));
}

void test_alarm_disabled_does_not_ring(void) {
    Alarm alarm;
    alarm.setHour(7);
    alarm.setMinute(30);
    alarm.setEnabled(false);

    DateTime ringTime(2023, 10, 24, 7, 30, 0);
    TEST_ASSERT_FALSE(alarm.shouldRing(ringTime));
}

void test_alarm_snoozed_does_not_ring(void) {
    Alarm alarm;
    alarm.setHour(7);
    alarm.setMinute(30);
    alarm.setEnabled(true);
    alarm.setSnoozeState(true, 2000000000);

    DateTime ringTime(2023, 10, 24, 7, 30, 0);
    // Time matches and alarm is enabled, but snooze flag blocks ringing
    TEST_ASSERT_FALSE(alarm.shouldRing(ringTime));
}

void test_alarm_day_bitmasks(void) {
    Alarm alarm;
    alarm.setHour(8);
    alarm.setMinute(0);
    alarm.setEnabled(true);
    alarm.setDays(DAY_FRI); // Only Friday

    // 2023-10-27 was a Friday (dayOfTheWeek() == 5)
    DateTime fri(2023, 10, 27, 8, 0, 0);
    TEST_ASSERT_EQUAL(5, fri.dayOfTheWeek());
    TEST_ASSERT_TRUE(alarm.shouldRing(fri));

    // 2023-10-26 was Thursday (dayOfTheWeek() == 4)
    DateTime thu(2023, 10, 26, 8, 0, 0);
    TEST_ASSERT_FALSE(alarm.shouldRing(thu));

    // 2023-10-28 was Saturday (dayOfTheWeek() == 6)
    DateTime sat(2023, 10, 28, 8, 0, 0);
    TEST_ASSERT_FALSE(alarm.shouldRing(sat));

    // Multiple days: Weekdays (Mon-Fri)
    alarm.setDays(DAY_MON | DAY_TUE | DAY_WED | DAY_THU | DAY_FRI);
    DateTime mon(2023, 10, 23, 8, 0, 0);
    DateTime sun(2023, 10, 22, 8, 0, 0);
    TEST_ASSERT_TRUE(alarm.shouldRing(mon));
    TEST_ASSERT_TRUE(alarm.shouldRing(fri));
    TEST_ASSERT_FALSE(alarm.shouldRing(sun));
    TEST_ASSERT_FALSE(alarm.shouldRing(sat));
}

void test_alarm_one_time_dismiss(void) {
    Alarm alarm;
    alarm.setHour(7);
    alarm.setMinute(30);
    alarm.setEnabled(true);
    
    DateTime ringTime(2023, 10, 24, 7, 30, 0);
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
    alarm.setBiweeklyOddWeek(true); // Odd week alarm
    alarm.setEnabled(true);
    
    // Jan 2, 2024 is Tue in odd week 2817
    DateTime tue1(2024, 1, 2, 6, 0, 0);
    bool isOdd1 = Alarm::isOddWeek(tue1);
    TEST_ASSERT_TRUE(isOdd1);
    
    // Jan 9, 2024 is Tue in even week 2818
    DateTime tue2(2024, 1, 9, 6, 0, 0);
    bool isOdd2 = Alarm::isOddWeek(tue2);
    TEST_ASSERT_FALSE(isOdd2);
    
    // Odd week alarm should ring on tue1 and not on tue2
    TEST_ASSERT_TRUE(alarm.shouldRing(tue1));
    TEST_ASSERT_FALSE(alarm.shouldRing(tue2));
}

void test_alarm_biweekly_even_week(void) {
    Alarm alarm;
    alarm.setHour(6);
    alarm.setMinute(0);
    alarm.setDays(DAY_TUE);
    alarm.setBiweekly(true);
    alarm.setBiweeklyOddWeek(false); // Even week alarm
    alarm.setEnabled(true);

    DateTime tue1(2024, 1, 2, 6, 0, 0);
    DateTime tue2(2024, 1, 9, 6, 0, 0);

    // Even week alarm should NOT ring on tue1 and SHOULD ring on tue2
    TEST_ASSERT_FALSE(alarm.shouldRing(tue1));
    TEST_ASSERT_TRUE(alarm.shouldRing(tue2));
}

void test_alarm_dismiss_clears_snooze(void) {
    Alarm alarm;
    alarm.setHour(7);
    alarm.setMinute(30);
    alarm.setDays(DAY_MON | DAY_TUE);
    alarm.setEnabled(true);
    alarm.setSnoozeState(true, 1700000000);
    TEST_ASSERT_TRUE(alarm.isSnoozed());

    DateTime dismissTime(2023, 10, 24, 7, 35, 0);
    alarm.dismiss(dismissTime);
    TEST_ASSERT_FALSE(alarm.isSnoozed());
    TEST_ASSERT_EQUAL(0, alarm.getSnoozeUntil());
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
    return 0;
}

void setup() {
    delay(2000); // Give some time for the serial monitor to connect
    UNITY_BEGIN();
    RUN_TEST(test_alarm_default_constructor);
    RUN_TEST(test_alarm_set_hour_clamping);
    RUN_TEST(test_alarm_set_minute_clamping);
    RUN_TEST(test_alarm_id_and_enable);
    RUN_TEST(test_alarm_disabling_clears_snooze);
    RUN_TEST(test_alarm_update_snooze);
    RUN_TEST(test_alarm_disabled_does_not_ring);
    RUN_TEST(test_alarm_snoozed_does_not_ring);
    RUN_TEST(test_alarm_day_bitmasks);
    RUN_TEST(test_alarm_one_time_dismiss);
    RUN_TEST(test_alarm_repeating_dismiss);
    RUN_TEST(test_alarm_biweekly_parity);
    RUN_TEST(test_alarm_biweekly_even_week);
    RUN_TEST(test_alarm_dismiss_clears_snooze);
    RUN_TEST(test_alarm_dst_spring_forward_skipped_hour);
    RUN_TEST(test_alarm_dst_fall_back_dismiss_no_duplicate);
    RUN_TEST(test_alarm_biweekly_parity_across_dst);
    UNITY_END();
}

void loop() {
    // Empty loop
}
