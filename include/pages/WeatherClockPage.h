#pragma once

#include "Page.h"
#include <TFT_eSPI.h>
#include <cstdint>

struct WeatherClockDisplayData
{
  String time;
  String date;
  String dayOfWeek;
  float indoorTemp;
  float indoorHumidity;
  float outdoorTemp;
  String outdoorCondition;
  String tod;
  String seconds;
  String nextAlarm;
};

/**
 * @class WeatherClockPage
 * @brief A combo page that displays clock, outdoor weather, and indoor sensors.
 */
class WeatherClockPage : public Page
{
public:
  explicit WeatherClockPage(TFT_eSPI *tft);
  virtual ~WeatherClockPage();

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
  void drawWeather(TFT_eSPI &tft);

  // New drawing functions for the bottom row
  void drawIndoorTemp(TFT_eSPI &tft);
  void drawBottomAlarm(TFT_eSPI &tft);
  void drawIndoorHumidity(TFT_eSPI &tft);

  void drawSeconds(TFT_eSPI &tft);
  void updateSpriteColors();
  void initAlarmSprite(TFT_eSPI &tft);
  void updateDisplayData(WeatherClockDisplayData &data);

  // Flag to track sprite creation
  bool _spritesCreated = false;

  // Sprites
  TFT_eSprite _sprClock;
  TFT_eSprite _sprDayOfWeek;
  TFT_eSprite _sprDate;
  TFT_eSprite _sprWeather;        // Row 2
  TFT_eSprite _sprIndoorTemp;     // Row 4 Left
  TFT_eSprite _sprBottomAlarm;    // Row 4 Center
  TFT_eSprite _sprIndoorHumidity; // Row 4 Right
  TFT_eSprite _sprTOD;
  TFT_eSprite _sprSeconds;
  TFT_eSprite _alarmSprite; // Full screen overlay

  // Cached values
  WeatherClockDisplayData _lastData;

  // Layout position variables
  int _clockX;
  int _clockY;
  int _sideX;
  int _todX;
  int _todY;
  int _secondsX;
  int _secondsY;
  int _weatherY;
  int _dateY;
  int _sensorY;
  int _alarmSpriteX;
  int _alarmSpriteY;

  // Calculated widths
  int _sensorWidth;
  int _alarmWidth;

  bool _wasAlarmActive = false;

  // Cached background color
  uint16_t _bgColor;
  float _dismissProgress = 0.0f;
  TFT_eSPI *_tft;
};
