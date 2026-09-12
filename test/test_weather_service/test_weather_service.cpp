#include <unity.h>
#include "WeatherService.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_wind_direction_cardinal_points(void) {
    TEST_ASSERT_EQUAL_STRING("N", WeatherService::getWindDirectionStr(0).c_str());
    TEST_ASSERT_EQUAL_STRING("NE", WeatherService::getWindDirectionStr(45).c_str());
    TEST_ASSERT_EQUAL_STRING("E", WeatherService::getWindDirectionStr(90).c_str());
    TEST_ASSERT_EQUAL_STRING("SE", WeatherService::getWindDirectionStr(135).c_str());
    TEST_ASSERT_EQUAL_STRING("S", WeatherService::getWindDirectionStr(180).c_str());
    TEST_ASSERT_EQUAL_STRING("SW", WeatherService::getWindDirectionStr(225).c_str());
    TEST_ASSERT_EQUAL_STRING("W", WeatherService::getWindDirectionStr(270).c_str());
    TEST_ASSERT_EQUAL_STRING("NW", WeatherService::getWindDirectionStr(315).c_str());
    TEST_ASSERT_EQUAL_STRING("N", WeatherService::getWindDirectionStr(360).c_str());
}

void test_wind_direction_boundary_angles(void) {
    // 22.5 deg boundaries
    TEST_ASSERT_EQUAL_STRING("N", WeatherService::getWindDirectionStr(22).c_str());
    TEST_ASSERT_EQUAL_STRING("NE", WeatherService::getWindDirectionStr(23).c_str());

    TEST_ASSERT_EQUAL_STRING("NE", WeatherService::getWindDirectionStr(67).c_str());
    TEST_ASSERT_EQUAL_STRING("E", WeatherService::getWindDirectionStr(68).c_str());

    TEST_ASSERT_EQUAL_STRING("E", WeatherService::getWindDirectionStr(112).c_str());
    TEST_ASSERT_EQUAL_STRING("SE", WeatherService::getWindDirectionStr(113).c_str());

    TEST_ASSERT_EQUAL_STRING("SE", WeatherService::getWindDirectionStr(157).c_str());
    TEST_ASSERT_EQUAL_STRING("S", WeatherService::getWindDirectionStr(158).c_str());

    TEST_ASSERT_EQUAL_STRING("S", WeatherService::getWindDirectionStr(202).c_str());
    TEST_ASSERT_EQUAL_STRING("SW", WeatherService::getWindDirectionStr(203).c_str());

    TEST_ASSERT_EQUAL_STRING("SW", WeatherService::getWindDirectionStr(247).c_str());
    TEST_ASSERT_EQUAL_STRING("W", WeatherService::getWindDirectionStr(248).c_str());

    TEST_ASSERT_EQUAL_STRING("W", WeatherService::getWindDirectionStr(292).c_str());
    TEST_ASSERT_EQUAL_STRING("NW", WeatherService::getWindDirectionStr(293).c_str());

    TEST_ASSERT_EQUAL_STRING("NW", WeatherService::getWindDirectionStr(337).c_str());
    TEST_ASSERT_EQUAL_STRING("N", WeatherService::getWindDirectionStr(338).c_str());
}

void test_wind_direction_wrapping(void) {
    // Negative degrees
    TEST_ASSERT_EQUAL_STRING("W", WeatherService::getWindDirectionStr(-90).c_str());
    TEST_ASSERT_EQUAL_STRING("NW", WeatherService::getWindDirectionStr(-45).c_str());
    TEST_ASSERT_EQUAL_STRING("S", WeatherService::getWindDirectionStr(-180).c_str());

    // Greater than 360
    TEST_ASSERT_EQUAL_STRING("NE", WeatherService::getWindDirectionStr(405).c_str());
    TEST_ASSERT_EQUAL_STRING("N", WeatherService::getWindDirectionStr(720).c_str());
}

void test_wmo_weather_codes(void) {
    TEST_ASSERT_EQUAL_STRING("Clear", WeatherService::getConditionFromWMO(0));
    TEST_ASSERT_EQUAL_STRING("Mainly Clear", WeatherService::getConditionFromWMO(1));
    TEST_ASSERT_EQUAL_STRING("Partly Cloudy", WeatherService::getConditionFromWMO(2));
    TEST_ASSERT_EQUAL_STRING("Overcast", WeatherService::getConditionFromWMO(3));
    TEST_ASSERT_EQUAL_STRING("Fog", WeatherService::getConditionFromWMO(45));
    TEST_ASSERT_EQUAL_STRING("Fog", WeatherService::getConditionFromWMO(48));
    TEST_ASSERT_EQUAL_STRING("Drizzle", WeatherService::getConditionFromWMO(51));
    TEST_ASSERT_EQUAL_STRING("Drizzle", WeatherService::getConditionFromWMO(53));
    TEST_ASSERT_EQUAL_STRING("Drizzle", WeatherService::getConditionFromWMO(55));
    TEST_ASSERT_EQUAL_STRING("Freezing Drizzle", WeatherService::getConditionFromWMO(56));
    TEST_ASSERT_EQUAL_STRING("Freezing Drizzle", WeatherService::getConditionFromWMO(57));
    TEST_ASSERT_EQUAL_STRING("Rain", WeatherService::getConditionFromWMO(61));
    TEST_ASSERT_EQUAL_STRING("Rain", WeatherService::getConditionFromWMO(63));
    TEST_ASSERT_EQUAL_STRING("Rain", WeatherService::getConditionFromWMO(65));
    TEST_ASSERT_EQUAL_STRING("Freezing Rain", WeatherService::getConditionFromWMO(66));
    TEST_ASSERT_EQUAL_STRING("Freezing Rain", WeatherService::getConditionFromWMO(67));
    TEST_ASSERT_EQUAL_STRING("Snow", WeatherService::getConditionFromWMO(71));
    TEST_ASSERT_EQUAL_STRING("Snow", WeatherService::getConditionFromWMO(73));
    TEST_ASSERT_EQUAL_STRING("Snow", WeatherService::getConditionFromWMO(75));
    TEST_ASSERT_EQUAL_STRING("Snow Grains", WeatherService::getConditionFromWMO(77));
    TEST_ASSERT_EQUAL_STRING("Rain Showers", WeatherService::getConditionFromWMO(80));
    TEST_ASSERT_EQUAL_STRING("Rain Showers", WeatherService::getConditionFromWMO(81));
    TEST_ASSERT_EQUAL_STRING("Rain Showers", WeatherService::getConditionFromWMO(82));
    TEST_ASSERT_EQUAL_STRING("Snow Showers", WeatherService::getConditionFromWMO(85));
    TEST_ASSERT_EQUAL_STRING("Snow Showers", WeatherService::getConditionFromWMO(86));
    TEST_ASSERT_EQUAL_STRING("Thunderstorm", WeatherService::getConditionFromWMO(95));
    TEST_ASSERT_EQUAL_STRING("Thunderstorm", WeatherService::getConditionFromWMO(96));
    TEST_ASSERT_EQUAL_STRING("Thunderstorm", WeatherService::getConditionFromWMO(99));

    // Unknown codes
    TEST_ASSERT_EQUAL_STRING("Unknown", WeatherService::getConditionFromWMO(-1));
    TEST_ASSERT_EQUAL_STRING("Unknown", WeatherService::getConditionFromWMO(4));
    TEST_ASSERT_EQUAL_STRING("Unknown", WeatherService::getConditionFromWMO(100));
}

void test_url_encode(void) {
    // Basic text
    TEST_ASSERT_EQUAL_STRING("Boston", WeatherService::urlEncode("Boston").c_str());

    // Empty string
    TEST_ASSERT_EQUAL_STRING("", WeatherService::urlEncode("").c_str());

    // Space to plus
    TEST_ASSERT_EQUAL_STRING("New+York", WeatherService::urlEncode("New York").c_str());

    // Special characters
    TEST_ASSERT_EQUAL_STRING("New+York%2C+NY", WeatherService::urlEncode("New York, NY").c_str());

    // Hyphen, underscore, dot, tilde preserved (RFC 3986 unreserved)
    TEST_ASSERT_EQUAL_STRING("saint-louis_area.v1~test", WeatherService::urlEncode("saint-louis_area.v1~test").c_str());

    // Free function wrapper
    TEST_ASSERT_EQUAL_STRING("Boston", urlEncode("Boston").c_str());
}

void test_check_word_presence(void) {
    // Word at start
    TEST_ASSERT_TRUE(WeatherService::checkWordPresence("ny clock", "ny"));

    // Word at end
    TEST_ASSERT_TRUE(WeatherService::checkWordPresence("clock in ny", "ny"));

    // Word in middle surrounded by punctuation/spaces
    TEST_ASSERT_TRUE(WeatherService::checkWordPresence("new york, ny, usa", "ny"));

    // Exact string
    TEST_ASSERT_TRUE(WeatherService::checkWordPresence("ny", "ny"));

    // Empty text
    TEST_ASSERT_FALSE(WeatherService::checkWordPresence("", "ny"));

    // Word with exclamation point
    TEST_ASSERT_TRUE(WeatherService::checkWordPresence("Hello, world!", "world"));

    // Embedded substring inside word should NOT match
    TEST_ASSERT_FALSE(WeatherService::checkWordPresence("company", "ny"));
    TEST_ASSERT_FALSE(WeatherService::checkWordPresence("nyc", "ny"));
    TEST_ASSERT_FALSE(WeatherService::checkWordPresence("many", "ny"));

    // Free function wrapper
    TEST_ASSERT_TRUE(checkWordPresence("new york, ny", "ny"));
}

void test_weather_data_initial_state(void) {
    WeatherData data;
    TEST_ASSERT_FALSE(data.isValid);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, data.temp);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, data.feelsLike);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, data.humidity);
    TEST_ASSERT_EQUAL(0, data.rainChance);
    TEST_ASSERT_EQUAL_STRING("", data.condition);
}

int main(int argc, char **argv) {
    return 0;
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_wind_direction_cardinal_points);
    RUN_TEST(test_wind_direction_boundary_angles);
    RUN_TEST(test_wind_direction_wrapping);
    RUN_TEST(test_wmo_weather_codes);
    RUN_TEST(test_url_encode);
    RUN_TEST(test_check_word_presence);
    RUN_TEST(test_weather_data_initial_state);
    UNITY_END();
}

void loop() {
    // Empty loop
}
