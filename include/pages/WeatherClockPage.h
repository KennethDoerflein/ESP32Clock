#pragma once

#include "Page.h"
#include <TFT_eSPI.h>
#include <cstdint>

struct WeatherClockDisplayData
{
  String time;
  String date;
  String dayOfWeek;
  float temp; // Can be indoor or outdoor
  bool isIndoorTemp;
  float humidity;   // For indoor
  String condition; // For outdoor
  bool showIndoor;  // Which mode are we in?
  String tod;
  String seconds;
  String nextAlarm1;
  String nextAlarm2;
};

/**
 * @class WeatherClockPage
 * @brief A combo page that alternates between clock/internal sensors and outdoor weather.
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
  void drawTemperature(TFT_eSPI &tft);
  void drawRightElement(TFT_eSPI &tft); // Humidity or Condition
  void drawSeconds(TFT_eSPI &tft);
  void drawNextAlarms(TFT_eSPI &tft, const String &alarm1, const String &alarm2);
  void updateSpriteColors();
  void initAlarmSprite(TFT_eSPI &tft);
  void updateDisplayData(WeatherClockDisplayData &data);

  // Flag to track sprite creation
  bool _spritesCreated = false;

  // Sprites
  TFT_eSprite _sprClock;
  TFT_eSprite _sprDayOfWeek;
  TFT_eSprite _sprDate;
  TFT_eSprite _sprTemp;
  TFT_eSprite _sprRightElement; // Replaces _sprHumidity, used for both Humidity and Condition
  TFT_eSprite _sprTOD;
  TFT_eSprite _sprSeconds;
  TFT_eSprite _alarmSprite;
  TFT_eSprite _sprNextAlarm1;
  TFT_eSprite _sprNextAlarm2;

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
  int _dateY;
  int _sensorY;
  int _alarmRowY;
  int _alarmSpriteX;
  int _alarmSpriteY;

  bool _wasAlarmActive = false;

  // Alternating logic
  bool _showIndoor = true;
  unsigned long _lastToggleTime = 0;
  const unsigned long TOGGLE_INTERVAL = 5000; // 5 seconds

  // Cached background color
  uint16_t _bgColor;
  float _dismissProgress = 0.0f;
  TFT_eSPI *_tft;
};
