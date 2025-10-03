#pragma once

#include <TFT_eSPI.h>

class Display
{
public:
  static Display &getInstance()
  {
    static Display instance;
    return instance;
  }

  void begin();
  void drawClock(const char *timeStr);
  void drawDate(const char *dateStr);
  void drawDayOfWeek(const char *dayStr);
  void drawSensors(float temp, float humidity);

private:
  Display() {} // Private constructor for singleton

  TFT_eSPI tft;
  TFT_eSprite sprClock{&tft};
  TFT_eSprite sprDate{&tft};
  TFT_eSprite sprDay{&tft};
  TFT_eSprite sprSensor{&tft};

  void setupSprites();
};
