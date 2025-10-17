#pragma once

#include "Page.h"
#include <TFT_eSPI.h>

/**
 * @class ClockPage
 * @brief A page that displays the main clock face, including time, date, and sensor data.
 *
 * This class implements the Page interface to render the primary UI of the clock.
 * It manages its own set of sprites for displaying different pieces of information.
 */
class ClockPage : public Page
{
public:
  explicit ClockPage(TFT_eSPI *tft);
  virtual ~ClockPage();

  void onEnter(TFT_eSPI &tft) override;
  void onExit() override;
  void update() override;
  void render(TFT_eSPI &tft) override;
  void refresh() override;

private:
  void setupSprites(TFT_eSPI &tft);
  void setupLayout(TFT_eSPI &tft);
  void drawClock(TFT_eSPI &tft);
  void drawDate(TFT_eSPI &tft);
  void drawDayOfWeek(TFT_eSPI &tft);
  void drawTemperature(TFT_eSPI &tft);
  void drawHumidity(TFT_eSPI &tft);

  // Flag to track sprite creation
  bool _spritesCreated = false;

  // Sprites for this page, initialized in the constructor
  TFT_eSprite sprClock;
  TFT_eSprite sprDayOfWeek;
  TFT_eSprite sprDate;
  TFT_eSprite sprTemp;
  TFT_eSprite sprHumidity;
  TFT_eSprite sprTOD;
  TFT_eSprite sprSeconds;

  // Cached values to prevent unnecessary redraws
  String lastTime;
  String lastDate;
  String lastDayOfWeek;
  String lastTOD;
  String lastSeconds;
  float lastTemp = -999;
  float lastHumidity = -999;

  // Layout position variables
  int clockY;
  int dateY;
  int sensorY;
};