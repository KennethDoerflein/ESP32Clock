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
  void drawClock(const char *timeStr, const char *todStr);
  void drawDate(const char *dateStr);
  void drawDayOfWeek(const char *dayStr);
  void drawTemperature(float temp, bool isCelsius);
  void drawHumidity(float humidity);
  void drawStatusMessage(const char *message);
  void drawLayout();
  void updateBrightness();

private:
  Display() {} // Private constructor for singleton

  TFT_eSPI tft;
  TFT_eSprite sprClock{&tft};
  TFT_eSprite sprDayOfWeek{&tft};
  TFT_eSprite sprDate{&tft};
  TFT_eSprite sprTemp{&tft};
  TFT_eSprite sprHumidity{&tft};
  TFT_eSprite sprTOD{&tft}; // Time of Day (AM/PM)

  void setupSprites();
  void setupLayout();
  void updateSprite(TFT_eSprite &sprite, const char *text, int x, int y);

  // --- Layout variables ---
  String lastTOD = "--";

  int clockY;
  int dateY;
  int sensorY;
};