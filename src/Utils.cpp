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

/**
 * @brief Validates a hostname string.
 *
 * Checks if the hostname follows standard rules (RFC 1123).
 * - Length between 1 and 63 characters.
 * - Contains only alphanumeric characters and hyphens.
 * - Cannot start or end with a hyphen.
 *
 * @param hostname The hostname string to validate.
 * @return True if valid, false otherwise.
 */
bool isValidHostname(const String &hostname)
{
  if (hostname.length() < 1 || hostname.length() > 63)
  {
    return false;
  }

  // Check for leading or trailing hyphen
  char first = hostname.charAt(0);
  char last = hostname.charAt(hostname.length() - 1);
  if (first == '-' || last == '-')
  {
    return false;
  }

  for (unsigned int i = 0; i < hostname.length(); i++)
  {
    char c = hostname.charAt(i);
    // Allow A-Z, a-z, 0-9, and hyphen
    if (!isalnum(c) && c != '-')
    {
      return false;
    }
  }

  return true;
}
