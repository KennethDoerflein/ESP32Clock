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
  String nextAlarm1;
  String nextAlarm2;
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

  virtual void onEnter(TFT_eSPI &tft) override;
  virtual void onExit() override;
  virtual void update() override;
  virtual void render(TFT_eSPI &tft) override;
  virtual void refresh(TFT_eSPI &tft, bool fullRefresh) override;

protected:
  virtual void setupSprites(TFT_eSPI &tft);
  virtual void setupLayout(TFT_eSPI &tft);

  // Helper for partial setup
  void setupClockSprites(TFT_eSPI &tft);
  void setupSensorSprites(TFT_eSPI &tft);
  void setupClockLayout(TFT_eSPI &tft);

  // Drawing methods
  virtual void drawClock(TFT_eSPI &tft);
  virtual void drawDate(TFT_eSPI &tft);
  virtual void drawDayOfWeek(TFT_eSPI &tft);
  virtual void drawTemperature(TFT_eSPI &tft);
  virtual void drawHumidity(TFT_eSPI &tft);
  virtual void drawSeconds(TFT_eSPI &tft);
  virtual void drawNextAlarms(TFT_eSPI &tft, const String &alarm1, const String &alarm2);

  virtual void updateSpriteColors();
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
  TFT_eSprite _sprNextAlarm1;
  TFT_eSprite _sprNextAlarm2;

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
  int _alarmRowY;

  // Cached background color
  uint16_t _bgColor;
  TFT_eSPI *_tft;
};
