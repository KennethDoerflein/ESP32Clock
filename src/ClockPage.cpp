// Comment out the next line to disable sprite borders for debugging
// #define DEBUG_BORDERS

#include "pages/ClockPage.h"
#include "constants.h"
#include "Utils.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "SensorModule.h"
#include "AlarmManager.h"
#include "fonts/DSEG7ModernBold104.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold48.h"
#include "fonts/DSEG7ModernBold48.h"
#include "fonts/CenturyGothicBold48.h"

#include <Arduino.h>

ClockPage::ClockPage(TFT_eSPI *tft)
    : _sprClock(tft), _sprDayOfWeek(tft), _sprDate(tft), _sprTemp(tft), _sprHumidity(tft), _sprTOD(tft), _sprSeconds(tft),
      _alarmSprite(tft), _tft(tft)
{
  // Sprites are initialized in the member initializer list
}

ClockPage::~ClockPage()
{
  // Sprites with TFT_eSPI driver reference are destructed automatically
}

void ClockPage::onEnter(TFT_eSPI &tft)
{
  updateSpriteColors();
  tft.fillScreen(_bgColor);

  if (!_spritesCreated)
  {
    setupSprites(tft);
    initAlarmSprite(tft);
    _spritesCreated = true;
  }
  setupLayout(tft);

  // Force a redraw of all elements
  _lastData = {};
}

void ClockPage::onExit()
{
  // Sprites are no longer deleted on exit to preserve them.
}

void ClockPage::update()
{
  // The rendering logic handles this currently.
}

void ClockPage::render(TFT_eSPI &tft)
{
  DisplayData currentData;
  updateDisplayData(currentData);

  updateAlarmSprite();
  if (currentData.seconds != _lastData.seconds)
  {
    drawSeconds(tft);
    _lastData.seconds = currentData.seconds;
  }
  if (fabs(currentData.temp - _lastData.temp) > 0.1)
  {
    drawTemperature(tft);
    _lastData.temp = currentData.temp;
  }
  if (fabs(currentData.humidity - _lastData.humidity) > 0.1)
  {
    drawHumidity(tft);
    _lastData.humidity = currentData.humidity;
  }
  if (currentData.time != _lastData.time || currentData.tod != _lastData.tod)
  {
    drawClock(tft);
    _lastData.time = currentData.time;
    _lastData.tod = currentData.tod;
  }
  if (currentData.date != _lastData.date)
  {
    drawDate(tft);
    _lastData.date = currentData.date;
  }
  if (currentData.dayOfWeek != _lastData.dayOfWeek)
  {
    drawDayOfWeek(tft);
    _lastData.dayOfWeek = currentData.dayOfWeek;
  }
}

void ClockPage::setupLayout(TFT_eSPI &tft)
{
  int screenWidth = tft.width();
  int screenHeight = tft.height();

  // --- Clock Block Layout ---
  _clockY = MARGIN;

  // Calculate the width of the side elements column (AM/PM and seconds)
  int sideWidth = max(_sprTOD.width(), _sprSeconds.width());
  int timeSideGap = 15; // Gap between HH:MM and the side elements

  // Calculate the total width of the entire clock display
  int totalWidth = CLOCK_SPRITE_WIDTH + timeSideGap + sideWidth;

  // Calculate the starting X position to center the clock display
  int startX = (screenWidth - totalWidth) / 2;
  if (startX < 0)
    startX = 0;

  // Set the position for the main clock sprite
  _clockX = startX - 15;

  // Calculate the starting X for the side elements
  _sideX = _clockX + CLOCK_SPRITE_WIDTH + timeSideGap;

  // Calculate the total height of the side elements stack
  int sideElementsVGap = 15;
  int sideElementsHeight = TOD_SPRITE_HEIGHT + sideElementsVGap + SECONDS_SPRITE_HEIGHT;

  // Calculate the starting Y to vertically center the side elements against the main clock
  int sideStartY = _clockY + (CLOCK_SPRITE_HEIGHT - sideElementsHeight) / 2;
  if (sideStartY < _clockY)
    sideStartY = _clockY;

  // Set positions for AM/PM and Seconds sprites
  _todX = _sideX + sideElementsVGap;
  _todY = sideStartY;
  _secondsX = _todX + (TOD_SPRITE_WIDTH - SECONDS_SPRITE_WIDTH) / 2;
  _secondsY = _todY + TOD_SPRITE_HEIGHT + sideElementsVGap + 3;

  // --- Bottom Rows Layout (Date and Sensors) ---
  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();
  _dateY = screenHeight - (fontHeight * 2 + MARGIN + 40);
  _sensorY = screenHeight - (fontHeight + MARGIN + 10);

  // --- Alarm Sprite Layout ---
  _alarmSpriteX = (screenWidth - _alarmSprite.width()) / 2;
  _alarmSpriteY = (screenHeight / 2) - (_alarmSprite.height() / 2);
}

void ClockPage::clearAlarmSprite()
{
  _tft->fillRect(_alarmSpriteX, _alarmSpriteY, _alarmSprite.width(), _alarmSprite.height(), _bgColor);
}

void ClockPage::setupSprites(TFT_eSPI &tft)
{
  // Create the sprites and configure them
  _sprClock.createSprite(CLOCK_SPRITE_WIDTH, CLOCK_SPRITE_HEIGHT);
  _sprClock.loadFont(DSEG7ModernBold104);
  _sprClock.setTextDatum(MR_DATUM);

  _sprTOD.createSprite(TOD_SPRITE_WIDTH, TOD_SPRITE_HEIGHT);
  _sprTOD.loadFont(DSEG14ModernBold32);
  _sprTOD.setTextDatum(TR_DATUM);

  _sprSeconds.createSprite(SECONDS_SPRITE_WIDTH, SECONDS_SPRITE_HEIGHT);
  _sprSeconds.loadFont(DSEG7ModernBold48);
  _sprSeconds.setTextDatum(TR_DATUM);

  _sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT);
  _sprDayOfWeek.loadFont(DSEG14ModernBold48);
  _sprDayOfWeek.setTextDatum(ML_DATUM);

  _sprDate.createSprite(tft.width() / 2 - MARGIN, DATE_SPRITE_HEIGHT);
  _sprDate.loadFont(DSEG14ModernBold48);
  _sprDate.setTextDatum(MR_DATUM);

  _sprTemp.createSprite(tft.width() / 2 - MARGIN, TEMP_SPRITE_HEIGHT);
  _sprTemp.loadFont(DSEG14ModernBold48);
  _sprTemp.setTextDatum(ML_DATUM);

  _sprHumidity.createSprite(tft.width() / 2 - MARGIN, HUMIDITY_SPRITE_HEIGHT);
  _sprHumidity.loadFont(DSEG14ModernBold48);
  _sprHumidity.setTextDatum(MR_DATUM);

  updateSpriteColors();
}

void ClockPage::updateSpriteColors()
{
  auto &config = ConfigManager::getInstance();
  _bgColor = hexToRGB565(config.getBackgroundColor());
  uint16_t timeColor = hexToRGB565(config.getTimeColor());
  uint16_t todColor = hexToRGB565(config.getTodColor());
  uint16_t secondsColor = hexToRGB565(config.getSecondsColor());
  uint16_t dayOfWeekColor = hexToRGB565(config.getDayOfWeekColor());
  uint16_t dateColor = hexToRGB565(config.getDateColor());
  uint16_t tempColor = hexToRGB565(config.getTempColor());
  uint16_t humidityColor = hexToRGB565(config.getHumidityColor());

  _sprClock.setTextColor(timeColor, _bgColor);
  _sprTOD.setTextColor(todColor, _bgColor);
  _sprSeconds.setTextColor(secondsColor, _bgColor);
  _sprDayOfWeek.setTextColor(dayOfWeekColor, _bgColor);
  _sprDate.setTextColor(dateColor, _bgColor);
  _sprTemp.setTextColor(tempColor, _bgColor);
  _sprHumidity.setTextColor(humidityColor, _bgColor);
}

void ClockPage::drawClock(TFT_eSPI &tft)
{
  auto &timeManager = TimeManager::getInstance();
  bool is24Hour = timeManager.is24HourFormat();
  String timeStr = timeManager.getFormattedTime();
  String todStr = timeManager.getTOD();

  _sprClock.fillSprite(_bgColor);
  _sprClock.drawString(timeStr.c_str(), _sprClock.width(), _sprClock.height() / 2);
#ifdef DEBUG_BORDERS
  _sprClock.drawRect(0, 0, _sprClock.width(), _sprClock.height(), TFT_RED);
#endif
  _sprClock.pushSprite(_clockX, _clockY);

  if (!is24Hour)
  {
    _sprTOD.fillSprite(_bgColor);
    _sprTOD.drawString(todStr.c_str(), _sprTOD.width(), 0);
#ifdef DEBUG_BORDERS
    _sprTOD.drawRect(0, 0, _sprTOD.width(), _sprTOD.height(), TFT_GREEN);
#endif
    _sprTOD.pushSprite(_todX, _todY);
  }
}

void ClockPage::drawSeconds(TFT_eSPI &tft)
{
  String secondsStr = TimeManager::getInstance().getFormattedSeconds();
  _sprSeconds.fillSprite(_bgColor);
  _sprSeconds.drawString(secondsStr.c_str(), _sprSeconds.width(), 0);
#ifdef DEBUG_BORDERS
  _sprSeconds.drawRect(0, 0, _sprSeconds.width(), _sprSeconds.height(), TFT_MAGENTA);
#endif
  _sprSeconds.pushSprite(_secondsX, _secondsY);
}

void ClockPage::drawDayOfWeek(TFT_eSPI &tft)
{
  String dayStr = TimeManager::getInstance().getDayOfWeek();
  _sprDayOfWeek.fillSprite(_bgColor);
  _sprDayOfWeek.drawString(dayStr.c_str(), 0, _sprDayOfWeek.height() / 2);
#ifdef DEBUG_BORDERS
  _sprDayOfWeek.drawRect(0, 0, _sprDayOfWeek.width(), _sprDayOfWeek.height(), TFT_BLUE);
#endif
  _sprDayOfWeek.pushSprite(MARGIN, _dateY);
}

void ClockPage::drawDate(TFT_eSPI &tft)
{
  String dateStr = TimeManager::getInstance().getFormattedDate();
  _sprDate.fillSprite(_bgColor);
  _sprDate.drawString(dateStr.c_str(), _sprDate.width(), _sprDate.height() / 2);
#ifdef DEBUG_BORDERS
  _sprDate.drawRect(0, 0, _sprDate.width(), _sprDate.height(), TFT_YELLOW);
#endif
  _sprDate.pushSprite(tft.width() / 2, _dateY);
}

void ClockPage::drawTemperature(TFT_eSPI &tft)
{
  float temp = getTemperature();
  auto &config = ConfigManager::getInstance();
  uint16_t tempColor = hexToRGB565(config.getTempColor());

  _sprTemp.fillSprite(_bgColor);
  _sprTemp.loadFont(DSEG14ModernBold48);

  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
  _sprTemp.drawString(tempBuf, 0, _sprTemp.height() / 2);

  int tempWidth = _sprTemp.textWidth(tempBuf);
  int fontHeight = _sprTemp.fontHeight();
  int circleRadius = max(2, fontHeight / 14);
  int circleX = tempWidth + circleRadius + 2;
  int circleY = (_sprTemp.height() / 2) - (fontHeight / 2) + circleRadius;
  _sprTemp.fillCircle(circleX, circleY, circleRadius, tempColor);

  _sprTemp.loadFont(DSEG14ModernBold32);
  _sprTemp.setTextDatum(TL_DATUM);
  char unit = config.isCelsius() ? 'C' : 'F';
  char unitBuf[2] = {unit, '\0'};
  int unitX = circleX + circleRadius + 2;
  int unitY = (_sprTemp.height() / 2) - (fontHeight / 2);
  _sprTemp.drawString(unitBuf, unitX, unitY);
  _sprTemp.setTextDatum(ML_DATUM);

#ifdef DEBUG_BORDERS
  _sprTemp.drawRect(0, 0, _sprTemp.width(), _sprTemp.height(), TFT_ORANGE);
#endif
  _sprTemp.pushSprite(MARGIN, _sensorY);
}

void ClockPage::drawHumidity(TFT_eSPI &tft)
{
  float humidity = getHumidity();
  char buf[24];
  if (humidity < 0)
  {
    snprintf(buf, sizeof(buf), "N/A");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%.0f%%", humidity);
  }

  _sprHumidity.fillSprite(_bgColor);
  _sprHumidity.drawString(buf, _sprHumidity.width(), _sprHumidity.height() / 2);

#ifdef DEBUG_BORDERS
  _sprHumidity.drawRect(0, 0, _sprHumidity.width(), _sprHumidity.height(), TFT_CYAN);
#endif
  _sprHumidity.pushSprite(tft.width() / 2, _sensorY);
}

void ClockPage::updateDisplayData(DisplayData &data)
{
  auto &timeManager = TimeManager::getInstance();
  data.time = timeManager.getFormattedTime();
  data.date = timeManager.getFormattedDate();
  data.dayOfWeek = timeManager.getDayOfWeek();
  data.temp = getTemperature();
  data.humidity = getHumidity();
  data.tod = timeManager.getTOD();
  data.seconds = timeManager.getFormattedSeconds();
}

void ClockPage::setDismissProgress(float progress)
{
  _dismissProgress = progress;
}

void ClockPage::refresh(TFT_eSPI &tft, bool fullRefresh)
{
  auto &config = ConfigManager::getInstance();
  updateSpriteColors();

  if (fullRefresh)
  {
    tft.fillScreen(_bgColor);
  }

  if (config.is24HourFormat())
  {
    // In 24-hour mode, ensure the TOD sprite is cleared immediately on refresh.
    _sprTOD.fillSprite(_bgColor);
    _sprTOD.pushSprite(_todX, _todY);
  }

  // Force a redraw of all elements by resetting their last known values
  _lastData = {};
}

void ClockPage::initAlarmSprite(TFT_eSPI &tft)
{
  tft.loadFont(CenturyGothicBold48);
  int textWidth = tft.textWidth("ALARM");
  _alarmSprite.createSprite(textWidth + ALARM_SPRITE_WIDTH_PADDING, ALARM_SPRITE_HEIGHT);
  _alarmSprite.loadFont(CenturyGothicBold48);
  _alarmSprite.setTextDatum(MC_DATUM);
  _alarmSprite.setTextColor(hexToRGB565(ConfigManager::getInstance().getAlarmTextColor().c_str()));
  tft.unloadFont();
}

void ClockPage::updateAlarmSprite()
{
  _alarmSprite.fillSprite(_bgColor);

  auto &alarmManager = AlarmManager::getInstance();
  auto &config = ConfigManager::getInstance();

  bool anySnoozed = false;
  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    if (config.getAlarm(i).isSnoozed())
    {
      anySnoozed = true;
      break;
    }
  }

  if (alarmManager.isRinging())
  {
    _alarmSprite.drawString("ALARM", _alarmSprite.width() / 2, _alarmSprite.height() / 2);
    if (_dismissProgress > 0.0f)
    {
      int barWidth = _alarmSprite.width() * _dismissProgress;
      _alarmSprite.fillRect(0, _alarmSprite.height() - ALARM_PROGRESS_BAR_HEIGHT, barWidth, ALARM_PROGRESS_BAR_HEIGHT, hexToRGB565(config.getAlarmTextColor().c_str()));
    }
  }
  else if (anySnoozed)
  {
    bool foundSnoozed = false;
    for (int i = 0; i < config.getNumAlarms(); ++i)
    {
      const auto &alarm = config.getAlarm(i);
      if (alarm.isSnoozed())
      {
        time_t snoozeUntil = alarm.getSnoozeUntil();
        time_t now = TimeManager::getInstance().getRTCTime().unixtime();
        long remaining = snoozeUntil - now;
        if (remaining < 0)
        {
          remaining = 0;
        }

        char buf[10];
        snprintf(buf, sizeof(buf), "%ld:%02ld", remaining / 60, remaining % 60);
        _alarmSprite.drawString(buf, _alarmSprite.width() / 2, _alarmSprite.height() / 2);
        foundSnoozed = true;
        break;
      }
    }
    if (!foundSnoozed)
    {
      // This case is reached if the snooze was just cancelled.
      // The sprite is explicitly cleared to ensure no stale countdown is shown.
      _alarmSprite.fillSprite(_bgColor);
    }

    // Also draw the progress bar if the button is being held
    if (_dismissProgress > 0.0f)
    {
      int barWidth = _alarmSprite.width() * _dismissProgress;
      _alarmSprite.fillRect(0, _alarmSprite.height() - ALARM_PROGRESS_BAR_HEIGHT, barWidth, ALARM_PROGRESS_BAR_HEIGHT, hexToRGB565(config.getAlarmTextColor().c_str()));
    }
  }
  else
  {
    _dismissProgress = 0.0f;
  }

  _alarmSprite.pushSprite(_alarmSpriteX, _alarmSpriteY);
}
