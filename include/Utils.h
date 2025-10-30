#pragma once

#include <Arduino.h>
#include <cstdint>

/**
 * @brief Converts a hexadecimal color string to a 16-bit RGB565 color value.
 *
 * This function takes a hex color string in the format "#RRGGBB" and
 * converts it into a 16-bit integer compatible with the TFT_eSPI library.
 *
 * @param hex The hexadecimal color string (e.g., "#FF00FF").
 * @return The 16-bit RGB565 color value.
 */
uint16_t hexToRGB565(String hex);
