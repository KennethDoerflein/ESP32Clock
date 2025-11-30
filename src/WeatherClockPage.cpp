/**
 * @file WeatherClockPage.cpp
 * @brief Implements the WeatherClockPage class for displaying the clock and alternating sensor/weather data.
 */

#include "pages/WeatherClockPage.h"
#include "Constants.h"
#include "Utils.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "SensorModule.h"
#include "AlarmManager.h"
#include "WeatherService.h"
#include "fonts/DSEG7ModernBold104.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold48.h"
#include "fonts/DSEG7ModernBold48.h"
#include "fonts/CenturyGothicBold48.h"

#include <Arduino.h>

WeatherClockPage::WeatherClockPage(TFT_eSPI *tft)
    : _sprClock(tft), _sprDayOfWeek(tft), _sprDate(tft), _sprTemp(tft), _sprRightElement(tft), _sprTOD(tft), _sprSeconds(tft),
      _alarmSprite(tft), _sprNextAlarm1(tft), _sprNextAlarm2(tft), _tft(tft)
{
}

WeatherClockPage::~WeatherClockPage()
{
}

void WeatherClockPage::onEnter(TFT_eSPI &tft)
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
  _showIndoor = true;
  _lastToggleTime = millis();
}

void WeatherClockPage::onExit()
{
}

void WeatherClockPage::update()
{
  // Rendering logic handles updates
}

void WeatherClockPage::render(TFT_eSPI &tft)
{
  // Handle toggling logic
  if (millis() - _lastToggleTime > TOGGLE_INTERVAL)
  {
    _showIndoor = !_showIndoor;
    _lastToggleTime = millis();
    // Force redraw of sensor row
    _lastData.temp = -999.0;
    _lastData.humidity = -999.0;
    _lastData.condition = " ";
    _lastData.showIndoor = !_showIndoor; // force mismatch
  }

  WeatherClockDisplayData currentData;
  updateDisplayData(currentData);

  if (currentData.seconds != _lastData.seconds)
  {
    drawSeconds(tft);
    _lastData.seconds = currentData.seconds;
  }

  // Always redraw temp/right element if mode changed
  if (currentData.showIndoor != _lastData.showIndoor)
  {
    drawTemperature(tft);
    drawRightElement(tft);
    _lastData.temp = currentData.temp;
    _lastData.humidity = currentData.humidity;
    _lastData.condition = currentData.condition;
    _lastData.showIndoor = currentData.showIndoor;
    _lastData.isIndoorTemp = currentData.isIndoorTemp;
  }
  else
  {
    // Check values if mode hasn't changed
    if (fabs(currentData.temp - _lastData.temp) > 0.1)
    {
      drawTemperature(tft);
      _lastData.temp = currentData.temp;
    }

    if (currentData.showIndoor)
    {
      if (fabs(currentData.humidity - _lastData.humidity) > 0.1)
      {
        drawRightElement(tft);
        _lastData.humidity = currentData.humidity;
      }
    }
    else
    {
      if (currentData.condition != _lastData.condition)
      {
        drawRightElement(tft);
        _lastData.condition = currentData.condition;
      }
    }
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
  if (currentData.nextAlarm1 != _lastData.nextAlarm1 || currentData.nextAlarm2 != _lastData.nextAlarm2)
  {
    drawNextAlarms(tft, currentData.nextAlarm1, currentData.nextAlarm2);
    _lastData.nextAlarm1 = currentData.nextAlarm1;
    _lastData.nextAlarm2 = currentData.nextAlarm2;
  }

  // Handle Alarm Sprite visibility
  auto &alarmManager = AlarmManager::getInstance();
  auto &config = ConfigManager::getInstance();
  bool isAlarmActive = alarmManager.isRinging() || config.isAnyAlarmSnoozed();

  if (isAlarmActive)
  {
    updateAlarmSprite();
    _wasAlarmActive = true;
  }
  else if (_wasAlarmActive)
  {
    clearAlarmSprite();
    drawNextAlarms(tft, currentData.nextAlarm1, currentData.nextAlarm2);
    _wasAlarmActive = false;
  }
}

void WeatherClockPage::setupLayout(TFT_eSPI &tft)
{
  int screenWidth = tft.width();
  int screenHeight = tft.height();

  _clockY = MARGIN;
  int sideWidth = max(_sprTOD.width(), _sprSeconds.width());
  int timeSideGap = 15;
  int totalWidth = CLOCK_SPRITE_WIDTH + timeSideGap + sideWidth;
  int startX = (screenWidth - totalWidth) / 2;
  if (startX < 0)
    startX = 0;

  _clockX = startX - 15;
  _sideX = _clockX + CLOCK_SPRITE_WIDTH + timeSideGap;

  int sideElementsVGap = 15;
  int sideElementsHeight = TOD_SPRITE_HEIGHT + sideElementsVGap + SECONDS_SPRITE_HEIGHT;
  int sideStartY = _clockY + (CLOCK_SPRITE_HEIGHT - sideElementsHeight) / 2;
  if (sideStartY < _clockY)
    sideStartY = _clockY;

  _todX = _sideX + sideElementsVGap;
  _todY = sideStartY;
  _secondsX = _todX + (TOD_SPRITE_WIDTH - SECONDS_SPRITE_WIDTH) / 2;
  _secondsY = _todY + TOD_SPRITE_HEIGHT + sideElementsVGap + 3;

  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();
  tft.unloadFont();
  _alarmRowY = screenHeight - (fontHeight * 3 + MARGIN + 80);
  _dateY = screenHeight - (fontHeight * 2 + MARGIN + 55);
  _sensorY = screenHeight - (fontHeight + MARGIN + 20);

  _alarmSpriteX = (screenWidth - _alarmSprite.width()) / 2;
  _alarmSpriteY = _sensorY + (TEMP_SPRITE_HEIGHT - ALARM_SPRITE_HEIGHT) / 2;
}

void WeatherClockPage::clearAlarmSprite()
{
  _tft->fillRect(_alarmSpriteX, _alarmSpriteY, _alarmSprite.width(), _alarmSprite.height(), _bgColor);
}

void WeatherClockPage::setupSprites(TFT_eSPI &tft)
{
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

  _sprNextAlarm1.createSprite(tft.width() / 2 - MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT);
  _sprNextAlarm1.loadFont(DSEG14ModernBold32);
  _sprNextAlarm1.setTextDatum(ML_DATUM);

  _sprNextAlarm2.createSprite(tft.width() / 2 - MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT);
  _sprNextAlarm2.loadFont(DSEG14ModernBold32);
  _sprNextAlarm2.setTextDatum(MR_DATUM);

  _sprTemp.createSprite(tft.width() / 2 - MARGIN, TEMP_SPRITE_HEIGHT);
  _sprTemp.loadFont(DSEG14ModernBold48);
  _sprTemp.setTextDatum(ML_DATUM);

  _sprRightElement.createSprite(tft.width() / 2 - MARGIN, HUMIDITY_SPRITE_HEIGHT);
  // Default font, will change based on content
  _sprRightElement.loadFont(DSEG14ModernBold48);
  _sprRightElement.setTextDatum(MR_DATUM);

  updateSpriteColors();
}

void WeatherClockPage::updateSpriteColors()
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
  uint16_t alarmColor = hexToRGB565(config.getAlarmTextColor());

  _sprClock.setTextColor(timeColor, _bgColor);
  _sprTOD.setTextColor(todColor, _bgColor);
  _sprSeconds.setTextColor(secondsColor, _bgColor);
  _sprDayOfWeek.setTextColor(dayOfWeekColor, _bgColor);
  _sprDate.setTextColor(dateColor, _bgColor);
  _sprTemp.setTextColor(tempColor, _bgColor);
  _sprRightElement.setTextColor(humidityColor, _bgColor);
  _sprNextAlarm1.setTextColor(alarmColor, _bgColor);
  _sprNextAlarm2.setTextColor(alarmColor, _bgColor);
}

void WeatherClockPage::drawClock(TFT_eSPI &tft)
{
  auto &timeManager = TimeManager::getInstance();
  bool is24Hour = timeManager.is24HourFormat();
  String timeStr = timeManager.getFormattedTime();
  String todStr = timeManager.getTOD();

  _sprClock.fillSprite(_bgColor);
  _sprClock.drawString(timeStr.c_str(), _sprClock.width(), _sprClock.height() / 2);
  _sprClock.pushSprite(_clockX, _clockY);

  if (!is24Hour)
  {
    _sprTOD.fillSprite(_bgColor);
    _sprTOD.drawString(todStr.c_str(), _sprTOD.width(), 0);
    _sprTOD.pushSprite(_todX, _todY);
  }
}

void WeatherClockPage::drawSeconds(TFT_eSPI &tft)
{
  String secondsStr = TimeManager::getInstance().getFormattedSeconds();
  _sprSeconds.fillSprite(_bgColor);
  _sprSeconds.drawString(secondsStr.c_str(), _sprSeconds.width(), 0);
  _sprSeconds.pushSprite(_secondsX, _secondsY);
}

void WeatherClockPage::drawDayOfWeek(TFT_eSPI &tft)
{
  String dayStr = TimeManager::getInstance().getDayOfWeek();
  _sprDayOfWeek.fillSprite(_bgColor);
  _sprDayOfWeek.drawString(dayStr.c_str(), 0, _sprDayOfWeek.height() / 2);
  _sprDayOfWeek.pushSprite(MARGIN, _dateY);
}

void WeatherClockPage::drawDate(TFT_eSPI &tft)
{
  String dateStr = TimeManager::getInstance().getFormattedDate();
  _sprDate.fillSprite(_bgColor);
  _sprDate.drawString(dateStr.c_str(), _sprDate.width(), _sprDate.height() / 2);
  _sprDate.pushSprite(_tft->width() / 2, _dateY);
}

void WeatherClockPage::drawNextAlarms(TFT_eSPI &tft, const String &alarm1, const String &alarm2)
{
  _sprNextAlarm1.fillSprite(_bgColor);
  if (alarm1.length() > 0)
  {
    _sprNextAlarm1.drawString(alarm1.c_str(), 0, _sprNextAlarm1.height() / 2);
  }
  _sprNextAlarm1.pushSprite(MARGIN, _alarmRowY);

  _sprNextAlarm2.fillSprite(_bgColor);
  if (alarm2.length() > 0)
  {
    _sprNextAlarm2.drawString(alarm2.c_str(), _sprNextAlarm2.width(), _sprNextAlarm2.height() / 2);
  }
  _sprNextAlarm2.pushSprite(_tft->width() / 2, _alarmRowY);
}

void WeatherClockPage::drawTemperature(TFT_eSPI &tft)
{
  auto &config = ConfigManager::getInstance();
  uint16_t tempColor = hexToRGB565(config.getTempColor());
  bool valid = true;

  float temp;
  if (_showIndoor)
  {
    temp = getTemperature();
  }
  else
  {
    WeatherData wd = WeatherService::getInstance().getCurrentWeather();
    if (wd.isValid)
    {
      temp = wd.temp;
      if (config.isCelsius())
      {
        temp = (temp - 32.0) * 5.0 / 9.0;
      }
    }
    else
    {
      valid = false;
    }
  }

  _sprTemp.fillSprite(_bgColor);
  _sprTemp.loadFont(DSEG14ModernBold48);

  if (valid)
  {
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
    _sprTemp.drawString(tempBuf, 0, _sprTemp.height() / 2);

    int tempWidth = _sprTemp.textWidth(tempBuf);
    int fontHeight = _sprTemp.fontHeight();
    int circleRadius = max(2, fontHeight / 14);
    int circleX = tempWidth + circleRadius + 2;
    int circleY = (_sprTemp.height() / 2) - (fontHeight / 2) + circleRadius;
    _sprTemp.fillCircle(circleX, circleY, circleRadius, tempColor);

    // Unit
    _sprTemp.loadFont(DSEG14ModernBold32);
    _sprTemp.setTextDatum(TL_DATUM);
    char unit = config.isCelsius() ? 'C' : 'F';
    char unitBuf[2] = {unit, '\0'};
    int unitX = circleX + circleRadius + 2;
    int unitY = (_sprTemp.height() / 2) - (fontHeight / 2);
    _sprTemp.drawString(unitBuf, unitX, unitY);

    // Indicator (IN/OUT)
    _sprTemp.unloadFont();
    _sprTemp.setTextFont(2);
    // Use TOD color for auxiliary info
    _sprTemp.setTextColor(hexToRGB565(config.getTodColor().c_str()), _bgColor);
    String label = _showIndoor ? "IN" : "OUT";
    _sprTemp.drawString(label, unitX + 25, unitY + 8, 2);

    _sprTemp.setTextDatum(ML_DATUM);
  }
  else
  {
    _sprTemp.drawString("N/A", 0, _sprTemp.height() / 2);
  }

  _sprTemp.pushSprite(MARGIN, _sensorY);
}

void WeatherClockPage::drawRightElement(TFT_eSPI &tft)
{
  _sprRightElement.fillSprite(_bgColor);

  if (_showIndoor)
  {
    // Draw Humidity
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

    _sprRightElement.loadFont(DSEG14ModernBold48);
    _sprRightElement.setTextDatum(MR_DATUM);
    _sprRightElement.drawString(buf, _sprRightElement.width(), _sprRightElement.height() / 2);
  }
  else
  {
    // Draw Condition
    WeatherData wd = WeatherService::getInstance().getCurrentWeather();
    String condition = wd.isValid ? wd.condition : "No Data";

    // Use normal font for text
    _sprRightElement.loadFont(CenturyGothicBold48);
    _sprRightElement.setTextDatum(MR_DATUM);
    int w = _sprRightElement.textWidth(condition);
    if (w > _sprRightElement.width())
    {
      _sprRightElement.unloadFont();
      _sprRightElement.setTextFont(4);
    }
    _sprRightElement.drawString(condition, _sprRightElement.width(), _sprRightElement.height() / 2);
  }

  _sprRightElement.pushSprite(_tft->width() / 2, _sensorY);
}

void WeatherClockPage::updateDisplayData(WeatherClockDisplayData &data)
{
  auto &timeManager = TimeManager::getInstance();
  data.time = timeManager.getFormattedTime();
  data.date = timeManager.getFormattedDate();
  data.dayOfWeek = timeManager.getDayOfWeek();

  data.showIndoor = _showIndoor;

  if (_showIndoor)
  {
    data.temp = getTemperature();
    data.humidity = getHumidity();
    data.isIndoorTemp = true;
  }
  else
  {
    data.temp = WeatherService::getInstance().getCurrentWeather().temp;
    data.condition = WeatherService::getInstance().getCurrentWeather().condition;
    data.isIndoorTemp = false;
  }

  data.tod = timeManager.getTOD();
  data.seconds = timeManager.getFormattedSeconds();

  std::vector<NextAlarmTime> alarms = timeManager.getNextAlarms(2);
  bool is24Hour = timeManager.is24HourFormat();

  if (alarms.size() > 0)
  {
    char timeStr[16];
    if (is24Hour)
    {
      sprintf(timeStr, "%02d:%02d", alarms[0].time.hour(), alarms[0].time.minute());
    }
    else
    {
      int hour12 = alarms[0].time.hour() % 12;
      if (hour12 == 0)
        hour12 = 12;
      const char *suffix = (alarms[0].time.hour() < 12) ? "AM" : "PM";
      sprintf(timeStr, "%d:%02d%s", hour12, alarms[0].time.minute(), suffix);
    }
    data.nextAlarm1 = String(timeStr);
  }
  else
  {
    data.nextAlarm1 = "";
  }

  if (alarms.size() > 1)
  {
    char timeStr[16];
    if (is24Hour)
    {
      sprintf(timeStr, "%02d:%02d", alarms[1].time.hour(), alarms[1].time.minute());
    }
    else
    {
      int hour12 = alarms[1].time.hour() % 12;
      if (hour12 == 0)
        hour12 = 12;
      const char *suffix = (alarms[1].time.hour() < 12) ? "AM" : "PM";
      sprintf(timeStr, "%d:%02d%s", hour12, alarms[1].time.minute(), suffix);
    }
    data.nextAlarm2 = String(timeStr);
  }
  else
  {
    data.nextAlarm2 = "";
  }
}

void WeatherClockPage::setDismissProgress(float progress)
{
  _dismissProgress = progress;
}

void WeatherClockPage::refresh(TFT_eSPI &tft, bool fullRefresh)
{
  auto &config = ConfigManager::getInstance();
  updateSpriteColors();

  if (fullRefresh)
  {
    tft.fillScreen(_bgColor);
  }

  if (config.is24HourFormat())
  {
    _sprTOD.fillSprite(_bgColor);
    _sprTOD.pushSprite(_todX, _todY);
  }

  // Force a redraw of all elements by setting their last known values to something invalid.
  _lastData.time = " ";
  _lastData.date = " ";
  _lastData.dayOfWeek = " ";
  _lastData.temp = -999.0;
  _lastData.humidity = -999.0;
  _lastData.condition = " ";
  _lastData.tod = " ";
  _lastData.seconds = " ";
  _lastData.nextAlarm1 = " ";
  _lastData.nextAlarm2 = " ";
}

void WeatherClockPage::initAlarmSprite(TFT_eSPI &tft)
{
  tft.loadFont(CenturyGothicBold48);
  int textWidth = tft.textWidth("ALARM");
  _alarmSprite.createSprite(textWidth + ALARM_SPRITE_WIDTH_PADDING, ALARM_SPRITE_HEIGHT);
  _alarmSprite.loadFont(CenturyGothicBold48);
  _alarmSprite.setTextDatum(MC_DATUM);
  _alarmSprite.setTextColor(hexToRGB565(ConfigManager::getInstance().getAlarmTextColor().c_str()));
  tft.unloadFont();
}

void WeatherClockPage::updateAlarmSprite()
{
  auto &config = ConfigManager::getInstance();
  uint16_t alarmColor = hexToRGB565(config.getAlarmTextColor().c_str());
  _alarmSprite.fillSprite(_bgColor);

  auto &alarmManager = AlarmManager::getInstance();
  bool showButton = false;
  String text = "";

  if (alarmManager.isRinging())
  {
    showButton = true;
    text = "ALARM";
  }
  else
  {
    for (int i = 0; i < config.getNumAlarms(); ++i)
    {
      const auto &alarm = config.getAlarmByIndex(i);
      if (alarm.isSnoozed())
      {
        time_t snoozeUntil = alarm.getSnoozeUntil();
        time_t now = TimeManager::getInstance().getRTCTime().unixtime();
        long remaining = snoozeUntil - now;
        if (remaining < 0)
          remaining = 0;
        char buf[10];
        snprintf(buf, sizeof(buf), "%ld:%02ld", remaining / 60, remaining % 60);
        text = String(buf);
        showButton = true;
        break;
      }
    }
  }

  if (showButton)
  {
    _alarmSprite.fillRoundRect(0, 0, _alarmSprite.width(), _alarmSprite.height(), 10, alarmColor);
    _alarmSprite.setTextColor(_bgColor);
    _alarmSprite.drawString(text.c_str(), _alarmSprite.width() / 2, _alarmSprite.height() / 2);

    if (_dismissProgress > 0.0f)
    {
      int margin = 5;
      int availableWidth = _alarmSprite.width() - (2 * margin);
      int barWidth = availableWidth * _dismissProgress;
      _alarmSprite.fillRoundRect(margin, _alarmSprite.height() - ALARM_PROGRESS_BAR_HEIGHT - margin, barWidth, ALARM_PROGRESS_BAR_HEIGHT, 3, TFT_WHITE);
    }
  }
  else
  {
    _dismissProgress = 0.0f;
  }

  _alarmSprite.pushSprite(_alarmSpriteX, _alarmSpriteY);
}
