#include "utils.h"

uint16_t hexToRGB565(String hex)
{
  long color = strtol(hex.c_str() + 1, NULL, 16);
  byte r = (color >> 16) & 0xFF;
  byte g = (color >> 8) & 0xFF;
  byte b = color & 0xFF;
  return ((r / 8) << 11) | ((g / 4) << 5) | (b / 8);
}
