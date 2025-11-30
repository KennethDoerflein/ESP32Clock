#pragma once

#include "ClockPage.h"
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
class WeatherClockPage : public ClockPage
{
public:
  explicit WeatherClockPage(TFT_eSPI *tft);
  virtual ~WeatherClockPage();

  void onEnter(TFT_eSPI &tft) override;
  void onExit() override;
  void update() override;
  void render(TFT_eSPI &tft) override;
  void refresh(TFT_eSPI &tft, bool fullRefresh) override;

private:
  void setupSprites(TFT_eSPI &tft) override;
  void setupLayout(TFT_eSPI &tft) override;

  void drawWeather(TFT_eSPI &tft);

  // New drawing functions for the bottom row
  void drawIndoorTemp(TFT_eSPI &tft);
  void drawBottomAlarm(TFT_eSPI &tft);
  void drawIndoorHumidity(TFT_eSPI &tft);

  void updateSpriteColors() override;
  void updateDisplayData(WeatherClockDisplayData &data);

  // Sprites
  TFT_eSprite _sprWeather;        // Row 2
  TFT_eSprite _sprIndoorTemp;     // Row 4 Left
  TFT_eSprite _sprBottomAlarm;    // Row 4 Center
  TFT_eSprite _sprIndoorHumidity; // Row 4 Right

  // Cached values
  WeatherClockDisplayData _lastData;

  // Layout position variables
  int _weatherY;

  // Calculated widths
  int _sensorWidth;
  int _alarmWidth;
};
