#include <unity.h>
#include <Arduino.h>
#include "LockGuard.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_lock_guard_null_mutex(void) {
    // Should safely do nothing when passed nullptr
    {
        LockGuard lock(nullptr);
    }
    TEST_ASSERT_TRUE(true);
}

void test_recursive_lock_guard_null_mutex(void) {
    // Should safely do nothing when passed nullptr
    {
        RecursiveLockGuard lock(nullptr);
    }
    TEST_ASSERT_TRUE(true);
}

void test_lock_guard_raii_acquisition_and_release(void) {
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(mutex);

    // Before lock: taking with 0 timeout must succeed
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, 0));
    xSemaphoreGive(mutex);

    // Inside LockGuard scope: mutex is taken
    {
        LockGuard lock(mutex);
        // Attempting to take again on standard non-recursive mutex should fail
        TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(mutex, 0));
    }

    // After LockGuard scope: mutex must be released
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, 0));
    xSemaphoreGive(mutex);

    vSemaphoreDelete(mutex);
}

void test_recursive_lock_guard_reentrant(void) {
    SemaphoreHandle_t recMutex = xSemaphoreCreateRecursiveMutex();
    TEST_ASSERT_NOT_NULL(recMutex);

    // Outer lock
    {
        RecursiveLockGuard lock1(recMutex);

        // Nested re-entrant lock on same thread must succeed
        {
            RecursiveLockGuard lock2(recMutex);
            TEST_ASSERT_TRUE(true);
        }

        // Still locked by lock1
    }

    // After all locks exit, taking should succeed
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTakeRecursive(recMutex, 0));
    xSemaphoreGiveRecursive(recMutex);

    vSemaphoreDelete(recMutex);
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_lock_guard_null_mutex);
    RUN_TEST(test_recursive_lock_guard_null_mutex);
    RUN_TEST(test_lock_guard_raii_acquisition_and_release);
    RUN_TEST(test_recursive_lock_guard_reentrant);
    UNITY_END();
}

void loop() {
    // Empty loop
}
