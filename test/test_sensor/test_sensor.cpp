#include <unity.h>
#include "SensorModule.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_corrected_humidity_zero_offset(void) {
    // Zero offset must return exact same humidity
    float corrected = calculateCorrectedHumidity(25.0f, 50.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, corrected);

    corrected = calculateCorrectedHumidity(10.0f, 80.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f, corrected);

    corrected = calculateCorrectedHumidity(35.0f, 20.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, corrected);
}

void test_corrected_humidity_zero_humidity_no_nan(void) {
    // Zero humidity must return 0.0f, never NaN (log(0) guard)
    float corrected = calculateCorrectedHumidity(25.0f, 0.0f, -2.0f);
    TEST_ASSERT_FALSE(isnan(corrected));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, corrected);

    corrected = calculateCorrectedHumidity(25.0f, 0.0f, 0.0f);
    TEST_ASSERT_FALSE(isnan(corrected));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, corrected);

    corrected = calculateCorrectedHumidity(25.0f, 0.0f, 5.0f);
    TEST_ASSERT_FALSE(isnan(corrected));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, corrected);
}

void test_corrected_humidity_cooling_offset(void) {
    // When enclosure is warmer than ambient, offset is negative (ambient is cooler).
    // Cooler air holds less moisture, so relative humidity must increase.
    float measuredTemp = 25.0f;
    float measuredHumidity = 50.0f;
    float offset = -3.0f;

    float corrected = calculateCorrectedHumidity(measuredTemp, measuredHumidity, offset);
    TEST_ASSERT_TRUE(corrected > measuredHumidity);
    // Physically expected at 22C vs 25C: ~59.9% RH
    TEST_ASSERT_FLOAT_WITHIN(1.5f, 59.9f, corrected);
}

void test_corrected_humidity_warming_offset(void) {
    // When offset is positive, ambient is warmer.
    // Warmer air has higher saturation capacity, so relative humidity must decrease.
    float measuredTemp = 20.0f;
    float measuredHumidity = 60.0f;
    float offset = 4.0f;

    float corrected = calculateCorrectedHumidity(measuredTemp, measuredHumidity, offset);
    TEST_ASSERT_TRUE(corrected < measuredHumidity);
}

void test_corrected_humidity_invalid_inputs(void) {
    // Invalid humidity (< 0 or > 100) must return original value unchanged
    TEST_ASSERT_EQUAL_FLOAT(-5.0f, calculateCorrectedHumidity(25.0f, -5.0f, -2.0f));
    TEST_ASSERT_EQUAL_FLOAT(-0.1f, calculateCorrectedHumidity(25.0f, -0.1f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(105.0f, calculateCorrectedHumidity(25.0f, 105.0f, -2.0f));
    TEST_ASSERT_EQUAL_FLOAT(200.0f, calculateCorrectedHumidity(25.0f, 200.0f, 2.0f));
}

void test_corrected_humidity_clamping(void) {
    // Large cooling offset on high humidity must clamp to 100%
    float clampedHigh = calculateCorrectedHumidity(30.0f, 95.0f, -15.0f);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, clampedHigh);

    // Large warming offset on near-zero humidity must clamp to >= 0%
    float clampedLow = calculateCorrectedHumidity(10.0f, 0.5f, 40.0f);
    TEST_ASSERT_TRUE(clampedLow >= 0.0f);
    TEST_ASSERT_TRUE(clampedLow <= 100.0f);
}

void test_temperature_conversion_functions(void) {
    // Water freezing point
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, celsiusToFahrenheit(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, fahrenheitToCelsius(32.0f));

    // Water boiling point
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.0f, celsiusToFahrenheit(100.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, fahrenheitToCelsius(212.0f));

    // Intersection point (-40C == -40F)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, celsiusToFahrenheit(-40.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, fahrenheitToCelsius(-40.0f));

    // Room temperature (25C == 77F)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 77.0f, celsiusToFahrenheit(25.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, fahrenheitToCelsius(77.0f));

    // Round-trip precision test
    for (float c = -20.0f; c <= 60.0f; c += 5.0f) {
        float f = celsiusToFahrenheit(c);
        float roundtrip = fahrenheitToCelsius(f);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, c, roundtrip);
    }
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_corrected_humidity_zero_offset);
    RUN_TEST(test_corrected_humidity_zero_humidity_no_nan);
    RUN_TEST(test_corrected_humidity_cooling_offset);
    RUN_TEST(test_corrected_humidity_warming_offset);
    RUN_TEST(test_corrected_humidity_invalid_inputs);
    RUN_TEST(test_corrected_humidity_clamping);
    RUN_TEST(test_temperature_conversion_functions);
    UNITY_END();
}

void loop() {
    // Empty loop
}
