#pragma once

#include "Page.h"
#include <TFT_eSPI.h>
#include <cstdint>

struct DisplayData
{
  String time;
  String date;
  String dayOfWeek;
  float temp;
  float humidity;
  String tod;
  String seconds;
};

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
  void refresh(TFT_eSPI &tft, bool fullRefresh) override;
  void setDismissProgress(float progress);
  void clearAlarmSprite();
  void updateAlarmSprite();

private:
  void setupSprites(TFT_eSPI &tft);
  void setupLayout(TFT_eSPI &tft);
  void drawClock(TFT_eSPI &tft);
  void drawDate(TFT_eSPI &tft);
  void drawDayOfWeek(TFT_eSPI &tft);
  void drawTemperature(TFT_eSPI &tft);
  void drawHumidity(TFT_eSPI &tft);
  void drawSeconds(TFT_eSPI &tft);
  void updateSpriteColors();
  void initAlarmSprite(TFT_eSPI &tft);
  void updateDisplayData(DisplayData &data);

  // Flag to track sprite creation
  bool _spritesCreated = false;

  // Sprites for this page, initialized in the constructor
  TFT_eSprite _sprClock;
  TFT_eSprite _sprDayOfWeek;
  TFT_eSprite _sprDate;
  TFT_eSprite _sprTemp;
  TFT_eSprite _sprHumidity;
  TFT_eSprite _sprTOD;
  TFT_eSprite _sprSeconds;
  TFT_eSprite _alarmSprite;

  // Cached values to prevent unnecessary redraws
  DisplayData _lastData;

  // Layout position variables
  int _clockX;
  int _clockY;
  int _sideX;
  int _todX;
  int _todY;
  int _secondsX;
  int _secondsY;
  int _dateY;
  int _sensorY;
  int _alarmSpriteX;
  int _alarmSpriteY;

  // Cached background color
  uint16_t _bgColor;
  float _dismissProgress = 0.0f;
  TFT_eSPI *_tft;
};