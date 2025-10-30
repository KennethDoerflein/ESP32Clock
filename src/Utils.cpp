/**
 * @file Utils.cpp
 * @brief Implements utility functions for the clock application.
 *
 * This file contains common helper functions that are used across different
 * parts of the firmware, such as color conversion utilities.
 */
#include "Utils.h"

/**
 * @brief Converts a hexadecimal color string to a 16-bit RGB565 color value.
 *
 * This function takes a hex color string in the format "#RRGGBB" and
 * converts it into a 16-bit integer compatible with the TFT_eSPI library.
 *
 * @param hex The hexadecimal color string (e.g., "#FF00FF").
 * @return The 16-bit RGB565 color value.
 */
uint16_t hexToRGB565(String hex)
{
  long color = strtol(hex.c_str() + 1, NULL, 16);
  byte r = (color >> 16) & 0xFF;
  byte g = (color >> 8) & 0xFF;
  byte b = color & 0xFF;
  return ((r / 8) << 11) | ((g / 4) << 5) | (b / 8);
}
