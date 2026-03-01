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
    : ClockPage(tft), // Base constructor
      _sprWeather(tft), _sprIndoorTemp(tft), _sprBottomAlarm(tft), _sprIndoorHumidity(tft)
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
    _spritesCreated = true;
  }

  setupLayout(tft); // Calculate layout after sprites are created

  // Force a redraw of all elements
  _lastWeatherData = {};
  _lastWeatherData.indoorTemp = -999.0;
  _lastWeatherData.indoorHumidity = -999.0;
  _lastWeatherData.outdoorTemp = -999.0;
  resetClockFields(_lastWeatherData);
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

  // Render shared clock elements (seconds, time/tod, date, dayOfWeek)
  renderClockElements(tft, currentData, _lastWeatherData);

  // Weather-specific elements
  if (fabs(currentData.outdoorTemp - _lastWeatherData.outdoorTemp) > 0.1 ||
      currentData.outdoorCondition != _lastWeatherData.outdoorCondition)
  {
    drawWeather(tft);
    _lastWeatherData.outdoorTemp = currentData.outdoorTemp;
    _lastWeatherData.outdoorCondition = currentData.outdoorCondition;
  }

  if (fabs(currentData.indoorTemp - _lastWeatherData.indoorTemp) > 0.1)
  {
    drawIndoorTemp(tft);
    _lastWeatherData.indoorTemp = currentData.indoorTemp;
  }

  if (strcmp(currentData.nextAlarm, _lastWeatherData.nextAlarm) != 0)
  {
    drawBottomAlarm(tft, currentData.nextAlarm);
    strncpy(_lastWeatherData.nextAlarm, currentData.nextAlarm, sizeof(_lastWeatherData.nextAlarm));
  }

  if (fabs(currentData.indoorHumidity - _lastWeatherData.indoorHumidity) > 0.1)
  {
    drawIndoorHumidity(tft);
    _lastWeatherData.indoorHumidity = currentData.indoorHumidity;
  }
}

void WeatherClockPage::setupLayout(TFT_eSPI &tft)
{
  // Call base class to setup Clock Header Layout
  setupClockLayout(tft);

  int screenWidth = tft.width();
  int screenHeight = tft.height();

  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();
  tft.unloadFont();

  _weatherY = screenHeight - (fontHeight * 3 + MARGIN + 80);
  _dateY = screenHeight - (fontHeight * 2 + MARGIN + 55);
  _sensorY = screenHeight - (fontHeight + MARGIN + 20);

  // Calculate width distribution for bottom row
  int availableWidth = screenWidth - 2 * MARGIN;
  _alarmWidth = availableWidth * 0.40;               // 40% for alarm to prevent overflow
  _sensorWidth = (availableWidth - _alarmWidth) / 2; // ~30% each for sensors
}

void WeatherClockPage::setupSprites(TFT_eSPI &tft)
{
  // Setup common sprites (Clock, Date, Day)
  setupClockSprites(tft);

  _sprWeather.createSprite(tft.width() - 2 * MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT);
  _sprWeather.loadFont(CenturyGothicBold48);
  _sprWeather.setTextDatum(MC_DATUM);

  // Bottom Row Sprites
  int screenWidth = tft.width();
  int availableWidth = screenWidth - 2 * MARGIN;
  int alarmWidth = availableWidth * 0.40;
  int sensorWidth = (availableWidth - alarmWidth) / 2;

  _sprIndoorTemp.createSprite(sensorWidth, TEMP_SPRITE_HEIGHT);
  _sprIndoorTemp.loadFont(DSEG14ModernBold48);
  _sprIndoorTemp.setTextDatum(ML_DATUM);

  _sprBottomAlarm.createSprite(alarmWidth, TEMP_SPRITE_HEIGHT);
  _sprBottomAlarm.loadFont(DSEG14ModernBold32);
  _sprBottomAlarm.setTextDatum(MC_DATUM);

  _sprIndoorHumidity.createSprite(sensorWidth, TEMP_SPRITE_HEIGHT);
  _sprIndoorHumidity.loadFont(DSEG14ModernBold48);
  _sprIndoorHumidity.setTextDatum(MR_DATUM);

  updateSpriteColors();
}

void WeatherClockPage::updateSpriteColors()
{
  // Update base colors
  ClockPage::updateSpriteColors();

  auto &config = ConfigManager::getInstance();
  uint16_t weatherTempColor = hexToRGB565(config.getWeatherTempColor());
  uint16_t weatherForecastColor = hexToRGB565(config.getWeatherForecastColor());
  uint16_t tempColor = hexToRGB565(config.getTempColor());
  uint16_t humidityColor = hexToRGB565(config.getHumidityColor());
  uint16_t alarmColor = hexToRGB565(config.getAlarmTextColor());

  _sprWeather.setTextColor(weatherTempColor, _bgColor);
  _sprIndoorTemp.setTextColor(tempColor, _bgColor);
  _sprBottomAlarm.setTextColor(alarmColor, _bgColor);
  _sprIndoorHumidity.setTextColor(humidityColor, _bgColor);
}

void WeatherClockPage::drawWeather(TFT_eSPI &tft)
{
  auto &config = ConfigManager::getInstance();

  WeatherData wd = WeatherService::getInstance().getCurrentWeather();
  uint16_t tempColor = hexToRGB565(config.getWeatherTempColor());
  uint16_t forecastColor = hexToRGB565(config.getWeatherForecastColor());

  _sprWeather.fillSprite(_bgColor);
  _sprWeather.setTextColor(tempColor, _bgColor);

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

    // Calculate widths to center everything as a block
    int tempW = _sprWeather.textWidth(tempBuf);

    _sprWeather.unloadFont();
    _sprWeather.setTextFont(4);
    int unitW = _sprWeather.textWidth(unit);
    int degreeW = 6; // Estimation for small circle spacing

    _sprWeather.loadFont(CenturyGothicBold48);
    int spaceW = _sprWeather.textWidth(" ");
    int condW = _sprWeather.textWidth(wd.condition);

    int totalW = tempW + degreeW + unitW + spaceW + condW;
    int startX = (_sprWeather.width() - totalW) / 2;
    if (startX < 0)
      startX = 0;

    int centerY = (_sprWeather.height() / 2) - 3;

    // Draw Temp
    _sprWeather.setTextColor(tempColor, _bgColor);
    _sprWeather.setTextDatum(ML_DATUM);
    _sprWeather.drawString(tempBuf, startX, centerY);

    // Draw Unit (Degree + C/F)
    int currentX = startX + tempW;

    // Degree Circle
    int circleRadius = 3;
    int circleX = currentX + circleRadius + 6;
    int degreeCenterY = centerY - 18;
    int unitTextY = centerY - 7;

    _sprWeather.fillCircle(circleX, degreeCenterY, circleRadius, tempColor);

    currentX += (circleRadius * 2) + 4;

    _sprWeather.unloadFont();
    _sprWeather.setTextFont(4);
    _sprWeather.setTextDatum(ML_DATUM);
    _sprWeather.drawString(unit, currentX + 8, unitTextY);

    currentX += unitW;

    // Draw Condition
    _sprWeather.loadFont(CenturyGothicBold48);
    _sprWeather.setTextColor(forecastColor, _bgColor);
    _sprWeather.setTextDatum(ML_DATUM);
    _sprWeather.drawString(" " + wd.condition, currentX, centerY);
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
  int circleX = tempWidth + circleRadius + 4;
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

void WeatherClockPage::drawBottomAlarm(TFT_eSPI &tft, const char *alarmText)
{
  _sprBottomAlarm.fillSprite(_bgColor);

  if (alarmText[0] != '\0')
  {
    _sprBottomAlarm.drawString(alarmText, _sprBottomAlarm.width() / 2, _sprBottomAlarm.height() / 2);
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
  fillClockDisplayBase(data);

  data.indoorTemp = getTemperature();
  data.indoorHumidity = getHumidity();

  WeatherData wd = WeatherService::getInstance().getCurrentWeather();
  data.outdoorTemp = wd.temp;
  data.outdoorCondition = wd.condition;

  auto &timeManager = TimeManager::getInstance();
  std::vector<NextAlarmTime> alarms = timeManager.getNextAlarms(1);
  bool is24Hour = timeManager.is24HourFormat();

  if (alarms.size() > 0)
  {
    if (is24Hour)
    {
      snprintf(data.nextAlarm, sizeof(data.nextAlarm), "%02d:%02d", alarms[0].time.hour(), alarms[0].time.minute());
    }
    else
    {
      int hour12 = alarms[0].time.hour() % 12;
      if (hour12 == 0)
        hour12 = 12;
      const char *suffix = (alarms[0].time.hour() < 12) ? "AM" : "PM";
      snprintf(data.nextAlarm, sizeof(data.nextAlarm), "%d:%02d%s", hour12, alarms[0].time.minute(), suffix);
    }
  }
  else
  {
    data.nextAlarm[0] = '\0';
  }
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

  // Force a redraw of all elements
  resetClockFields(_lastWeatherData);

  // WeatherClockPage-specific fields
  _lastWeatherData.indoorTemp = -999.0;
  _lastWeatherData.indoorHumidity = -999.0;
  _lastWeatherData.outdoorTemp = -999.0;
  _lastWeatherData.outdoorCondition = " ";
  strncpy(_lastWeatherData.nextAlarm, "REFRESH", sizeof(_lastWeatherData.nextAlarm));
}
