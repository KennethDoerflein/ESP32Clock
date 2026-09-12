#include <unity.h>
#include "TimeManager.h"
#include <algorithm>
#include <vector>

void setUp(void) {
}

void tearDown(void) {
}

void test_next_alarm_time_comparison(void) {
    DateTime t1(2024, 5, 10, 6, 30, 0);
    DateTime t2(2024, 5, 10, 7, 0, 0);
    DateTime t3(2024, 5, 11, 6, 30, 0);

    NextAlarmTime a1 = {t1, 1};
    NextAlarmTime a2 = {t2, 2};
    NextAlarmTime a3 = {t3, 3};

    TEST_ASSERT_TRUE(a1 < a2);
    TEST_ASSERT_TRUE(a2 < a3);
    TEST_ASSERT_TRUE(a1 < a3);

    TEST_ASSERT_FALSE(a2 < a1);
    TEST_ASSERT_FALSE(a1 < a1); // Strict weak ordering: not less than self

    // Vector sorting
    std::vector<NextAlarmTime> list = {a3, a1, a2};
    std::sort(list.begin(), list.end());

    TEST_ASSERT_EQUAL(1, list[0].id);
    TEST_ASSERT_EQUAL(2, list[1].id);
    TEST_ASSERT_EQUAL(3, list[2].id);
}

void test_format_time_12_hour(void) {
    char buf[16];

    // Midnight (0:00 -> "12:00")
    DateTime midnight(2024, 1, 1, 0, 0, 0);
    TimeManager::formatTime(buf, sizeof(buf), midnight, false);
    TEST_ASSERT_EQUAL_STRING("12:00", buf);

    // Morning with minute padding (1:05 -> "1:05")
    DateTime morning(2024, 1, 1, 1, 5, 0);
    TimeManager::formatTime(buf, sizeof(buf), morning, false);
    TEST_ASSERT_EQUAL_STRING("1:05", buf);

    // Noon (12:00 -> "12:00")
    DateTime noon(2024, 1, 1, 12, 0, 0);
    TimeManager::formatTime(buf, sizeof(buf), noon, false);
    TEST_ASSERT_EQUAL_STRING("12:00", buf);

    // Afternoon (13:30 -> "1:30")
    DateTime afternoon(2024, 1, 1, 13, 30, 0);
    TimeManager::formatTime(buf, sizeof(buf), afternoon, false);
    TEST_ASSERT_EQUAL_STRING("1:30", buf);

    // Night (23:59 -> "11:59")
    DateTime night(2024, 1, 1, 23, 59, 0);
    TimeManager::formatTime(buf, sizeof(buf), night, false);
    TEST_ASSERT_EQUAL_STRING("11:59", buf);
}

void test_format_time_24_hour(void) {
    char buf[16];

    // Midnight zero-padded ("00:00")
    DateTime midnight(2024, 1, 1, 0, 0, 0);
    TimeManager::formatTime(buf, sizeof(buf), midnight, true);
    TEST_ASSERT_EQUAL_STRING("00:00", buf);

    // Morning zero-padded ("07:05")
    DateTime morning(2024, 1, 1, 7, 5, 0);
    TimeManager::formatTime(buf, sizeof(buf), morning, true);
    TEST_ASSERT_EQUAL_STRING("07:05", buf);

    // Afternoon ("13:45")
    DateTime afternoon(2024, 1, 1, 13, 45, 0);
    TimeManager::formatTime(buf, sizeof(buf), afternoon, true);
    TEST_ASSERT_EQUAL_STRING("13:45", buf);

    // End of day ("23:59")
    DateTime night(2024, 1, 1, 23, 59, 0);
    TimeManager::formatTime(buf, sizeof(buf), night, true);
    TEST_ASSERT_EQUAL_STRING("23:59", buf);
}

void test_format_tod(void) {
    char buf[8];

    // AM period
    DateTime amTime(2024, 1, 1, 0, 0, 0);
    TimeManager::formatTOD(buf, sizeof(buf), amTime, false);
    TEST_ASSERT_EQUAL_STRING("AM", buf);

    DateTime am11(2024, 1, 1, 11, 59, 0);
    TimeManager::formatTOD(buf, sizeof(buf), am11, false);
    TEST_ASSERT_EQUAL_STRING("AM", buf);

    // PM period
    DateTime noon(2024, 1, 1, 12, 0, 0);
    TimeManager::formatTOD(buf, sizeof(buf), noon, false);
    TEST_ASSERT_EQUAL_STRING("PM", buf);

    DateTime pm23(2024, 1, 1, 23, 59, 0);
    TimeManager::formatTOD(buf, sizeof(buf), pm23, false);
    TEST_ASSERT_EQUAL_STRING("PM", buf);

    // 24-hour format suppresses AM/PM (empty string)
    TimeManager::formatTOD(buf, sizeof(buf), amTime, true);
    TEST_ASSERT_EQUAL_STRING("", buf);

    TimeManager::formatTOD(buf, sizeof(buf), noon, true);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_format_date_all_months(void) {
    char buf[16];
    const char *expectedMonths[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };

    for (int m = 1; m <= 12; m++) {
        DateTime dt(2024, m, 15, 12, 0, 0);
        TimeManager::formatDate(buf, sizeof(buf), dt);
        char expected[16];
        snprintf(expected, sizeof(expected), "%s 15", expectedMonths[m - 1]);
        TEST_ASSERT_EQUAL_STRING(expected, buf);
    }

    // Leap day
    DateTime leap(2024, 2, 29, 0, 0, 0);
    TimeManager::formatDate(buf, sizeof(buf), leap);
    TEST_ASSERT_EQUAL_STRING("FEB 29", buf);
}

void test_format_day_of_week_all_days(void) {
    char buf[8];

    // 2024-01-07 was Sunday (day 0)
    DateTime sun(2024, 1, 7, 0, 0, 0);
    TEST_ASSERT_EQUAL(0, sun.dayOfTheWeek());
    TimeManager::formatDayOfWeek(buf, sizeof(buf), sun);
    TEST_ASSERT_EQUAL_STRING("SUN", buf);

    // 2024-01-01 was Monday (day 1)
    DateTime mon(2024, 1, 1, 0, 0, 0);
    TEST_ASSERT_EQUAL(1, mon.dayOfTheWeek());
    TimeManager::formatDayOfWeek(buf, sizeof(buf), mon);
    TEST_ASSERT_EQUAL_STRING("MON", buf);

    // 2024-01-06 was Saturday (day 6)
    DateTime sat(2024, 1, 6, 0, 0, 0);
    TEST_ASSERT_EQUAL(6, sat.dayOfTheWeek());
    TimeManager::formatDayOfWeek(buf, sizeof(buf), sat);
    TEST_ASSERT_EQUAL_STRING("SAT", buf);
}

void test_format_seconds_padding(void) {
    char buf[8];

    DateTime s0(2024, 1, 1, 0, 0, 0);
    TimeManager::formatSeconds(buf, sizeof(buf), s0);
    TEST_ASSERT_EQUAL_STRING("00", buf);

    DateTime s7(2024, 1, 1, 0, 0, 7);
    TimeManager::formatSeconds(buf, sizeof(buf), s7);
    TEST_ASSERT_EQUAL_STRING("07", buf);

    DateTime s59(2024, 1, 1, 0, 0, 59);
    TimeManager::formatSeconds(buf, sizeof(buf), s59);
    TEST_ASSERT_EQUAL_STRING("59", buf);
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_next_alarm_time_comparison);
    RUN_TEST(test_format_time_12_hour);
    RUN_TEST(test_format_time_24_hour);
    RUN_TEST(test_format_tod);
    RUN_TEST(test_format_date_all_months);
    RUN_TEST(test_format_day_of_week_all_days);
    RUN_TEST(test_format_seconds_padding);
    UNITY_END();
}

void loop() {
    // Empty loop
}
