/**
 * @file WeatherClockPage.cpp
 * @brief Implements the WeatherClockPage class for displaying the clock and combined weather/sensor data.
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
    : _sprClock(tft), _sprDayOfWeek(tft), _sprDate(tft),
      _sprWeather(tft), _sprIndoorTemp(tft), _sprBottomAlarm(tft), _sprIndoorHumidity(tft),
      _sprTOD(tft), _sprSeconds(tft), _alarmSprite(tft), _tft(tft)
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

  setupLayout(tft); // Calculate layout after sprites are created

  // Force a redraw of all elements
  _lastData = {};
  _lastData.indoorTemp = -999.0;
  _lastData.indoorHumidity = -999.0;
  _lastData.outdoorTemp = -999.0;
  _lastData.time = " ";
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
  WeatherClockDisplayData currentData;
  updateDisplayData(currentData);

  if (currentData.seconds != _lastData.seconds)
  {
    drawSeconds(tft);
    _lastData.seconds = currentData.seconds;
  }

  // Draw Weather (Outdoor) if changed
  if (fabs(currentData.outdoorTemp - _lastData.outdoorTemp) > 0.1 ||
      currentData.outdoorCondition != _lastData.outdoorCondition)
  {
    drawWeather(tft);
    _lastData.outdoorTemp = currentData.outdoorTemp;
    _lastData.outdoorCondition = currentData.outdoorCondition;
  }

  // Draw Bottom Row Elements
  if (fabs(currentData.indoorTemp - _lastData.indoorTemp) > 0.1)
  {
    drawIndoorTemp(tft);
    _lastData.indoorTemp = currentData.indoorTemp;
  }

  if (currentData.nextAlarm != _lastData.nextAlarm)
  {
    drawBottomAlarm(tft);
    _lastData.nextAlarm = currentData.nextAlarm;
  }

  if (fabs(currentData.indoorHumidity - _lastData.indoorHumidity) > 0.1)
  {
    drawIndoorHumidity(tft);
    _lastData.indoorHumidity = currentData.indoorHumidity;
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
    drawWeather(tft);
    drawBottomAlarm(tft);
    _lastData.indoorTemp = -999.0; // Force full redraw of row
    _lastData.indoorHumidity = -999.0;
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

  _weatherY = screenHeight - (fontHeight * 3 + MARGIN + 80);
  _dateY = screenHeight - (fontHeight * 2 + MARGIN + 55);
  _sensorY = screenHeight - (fontHeight + MARGIN + 20);

  _alarmSpriteX = (screenWidth - _alarmSprite.width()) / 2;
  _alarmSpriteY = _sensorY + (TEMP_SPRITE_HEIGHT - ALARM_SPRITE_HEIGHT) / 2;

  // Calculate width distribution for bottom row
  int availableWidth = screenWidth - 2 * MARGIN;
  _alarmWidth = availableWidth * 0.40;               // 40% for alarm to prevent overflow
  _sensorWidth = (availableWidth - _alarmWidth) / 2; // ~30% each for sensors
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

  _sprWeather.createSprite(tft.width() - 2 * MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT);
  _sprWeather.loadFont(CenturyGothicBold48);
  _sprWeather.setTextDatum(MC_DATUM);

  // Bottom Row Sprites
  _sprIndoorTemp.createSprite(_sensorWidth, TEMP_SPRITE_HEIGHT);
  _sprIndoorTemp.loadFont(DSEG14ModernBold48);
  _sprIndoorTemp.setTextDatum(ML_DATUM);

  _sprBottomAlarm.createSprite(_alarmWidth, TEMP_SPRITE_HEIGHT);
  _sprBottomAlarm.loadFont(DSEG14ModernBold32);
  _sprBottomAlarm.setTextDatum(MC_DATUM);

  _sprIndoorHumidity.createSprite(_sensorWidth, TEMP_SPRITE_HEIGHT);
  _sprIndoorHumidity.loadFont(DSEG14ModernBold48);
  _sprIndoorHumidity.setTextDatum(MR_DATUM);

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

  _sprWeather.setTextColor(todColor, _bgColor);
  _sprIndoorTemp.setTextColor(tempColor, _bgColor);
  _sprBottomAlarm.setTextColor(alarmColor, _bgColor);
  _sprIndoorHumidity.setTextColor(humidityColor, _bgColor);
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

void WeatherClockPage::drawWeather(TFT_eSPI &tft)
{
  auto &config = ConfigManager::getInstance();

  WeatherData wd = WeatherService::getInstance().getCurrentWeather();
  uint16_t txtColor = hexToRGB565(config.getTodColor().c_str());

  _sprWeather.fillSprite(_bgColor);

  if (wd.isValid)
  {
    float temp = wd.temp;
    if (config.isCelsius())
    {
      temp = (temp - 32.0) * 5.0 / 9.0;
    }
    char tempBuf[10];
    snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
    String unit = config.isCelsius() ? "C" : "F";

    // Manual drawing with circle
    _sprWeather.loadFont(CenturyGothicBold48);
    _sprWeather.setTextDatum(ML_DATUM);

    int tempW = _sprWeather.textWidth(tempBuf);
    int spaceW = _sprWeather.textWidth(" ");
    int condW = _sprWeather.textWidth(wd.condition);
    int degreeW = 10;
    int unitW = _sprWeather.textWidth(unit);

    int totalW = tempW + degreeW + unitW + spaceW + condW;
    int startX = (_sprWeather.width() - totalW) / 2;
    if (startX < 0)
      startX = 0;

    _sprWeather.drawString(tempBuf, startX, _sprWeather.height() / 2);

    // Degree Circle
    int circleX = startX + tempW + 8;
    int circleY = _sprWeather.height() / 2 - 15;
    _sprWeather.fillCircle(circleX, circleY, 3, txtColor);

    _sprWeather.drawString(unit, startX + tempW + degreeW + 4, _sprWeather.height() / 2);
    _sprWeather.drawString(" " + wd.condition, startX + tempW + degreeW + unitW + 4, _sprWeather.height() / 2);
  }
  else
  {
    _sprWeather.loadFont(CenturyGothicBold48);
    _sprWeather.setTextDatum(MC_DATUM);
    _sprWeather.drawString("Weather N/A", _sprWeather.width() / 2, _sprWeather.height() / 2);
  }

  _sprWeather.pushSprite(MARGIN, _weatherY);
}

void WeatherClockPage::drawIndoorTemp(TFT_eSPI &tft)
{
  auto &config = ConfigManager::getInstance();
  uint16_t tempColor = hexToRGB565(config.getTempColor());
  float temp = getTemperature();

  _sprIndoorTemp.fillSprite(_bgColor);
  _sprIndoorTemp.loadFont(DSEG14ModernBold48);

  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
  _sprIndoorTemp.drawString(tempBuf, 0, _sprIndoorTemp.height() / 2);

  int tempWidth = _sprIndoorTemp.textWidth(tempBuf);
  int fontHeight = _sprIndoorTemp.fontHeight();
  int circleRadius = max(2, fontHeight / 14);
  int circleX = tempWidth + circleRadius + 8;
  int circleY = (_sprIndoorTemp.height() / 2) - (fontHeight / 2) + circleRadius;
  _sprIndoorTemp.fillCircle(circleX, circleY, circleRadius, tempColor);

  // Unit - Use Font 4
  _sprIndoorTemp.unloadFont();
  _sprIndoorTemp.setTextFont(4);
  char unit = config.isCelsius() ? 'C' : 'F';
  char unitBuf[2] = {unit, '\0'};
  // Adjust Y for Font 4
  _sprIndoorTemp.drawString(unitBuf, circleX + circleRadius + 6, (_sprIndoorTemp.height() / 2) - 10);

  _sprIndoorTemp.pushSprite(MARGIN, _sensorY);
}

void WeatherClockPage::drawBottomAlarm(TFT_eSPI &tft)
{
  auto &config = ConfigManager::getInstance();

  _sprBottomAlarm.fillSprite(_bgColor);

  std::vector<NextAlarmTime> alarms = TimeManager::getInstance().getNextAlarms(1);
  String alarmStr = "";
  if (alarms.size() > 0)
  {
    bool is24Hour = TimeManager::getInstance().is24HourFormat();
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
    alarmStr = String(timeStr);
  }

  if (alarmStr.length() > 0)
  {
    _sprBottomAlarm.loadFont(DSEG14ModernBold32);
    _sprBottomAlarm.setTextColor(hexToRGB565(config.getAlarmTextColor().c_str()), _bgColor);
    _sprBottomAlarm.drawString(alarmStr, _sprBottomAlarm.width() / 2, _sprBottomAlarm.height() / 2);
  }

  _sprBottomAlarm.pushSprite(MARGIN + _sensorWidth, _sensorY);
}

void WeatherClockPage::drawIndoorHumidity(TFT_eSPI &tft)
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

  _sprIndoorHumidity.fillSprite(_bgColor);
  _sprIndoorHumidity.loadFont(DSEG14ModernBold48);
  _sprIndoorHumidity.drawString(buf, _sprIndoorHumidity.width(), _sprIndoorHumidity.height() / 2);
  _sprIndoorHumidity.pushSprite(MARGIN + _sensorWidth + _alarmWidth, _sensorY);
}

void WeatherClockPage::updateDisplayData(WeatherClockDisplayData &data)
{
  auto &timeManager = TimeManager::getInstance();
  data.time = timeManager.getFormattedTime();
  data.date = timeManager.getFormattedDate();
  data.dayOfWeek = timeManager.getDayOfWeek();

  data.indoorTemp = getTemperature();
  data.indoorHumidity = getHumidity();

  WeatherData wd = WeatherService::getInstance().getCurrentWeather();
  data.outdoorTemp = wd.temp;
  data.outdoorCondition = wd.condition;

  data.tod = timeManager.getTOD();
  data.seconds = timeManager.getFormattedSeconds();

  std::vector<NextAlarmTime> alarms = timeManager.getNextAlarms(1);
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
    data.nextAlarm = String(timeStr);
  }
  else
  {
    data.nextAlarm = "";
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
  _lastData.indoorTemp = -999.0;
  _lastData.indoorHumidity = -999.0;
  _lastData.outdoorTemp = -999.0;
  _lastData.outdoorCondition = " ";
  _lastData.tod = " ";
  _lastData.seconds = " ";
  _lastData.nextAlarm = "REFRESH";
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
