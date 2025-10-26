// Comment out the next line to disable sprite borders for debugging
// #define DEBUG_BORDERS

#include "pages/ClockPage.h"
#include "utils.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "AlarmManager.h"
#include "sensors.h"
#include "fonts/DSEG7ModernBold104.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold48.h"
#include "fonts/DSEG7ModernBold48.h"
#include "fonts/CenturyGothicBold48.h"

#include <Arduino.h>

#define MARGIN 10


ClockPage::ClockPage(TFT_eSPI *tft)
    : sprClock(tft), sprDayOfWeek(tft), sprDate(tft), sprTemp(tft), sprHumidity(tft), sprTOD(tft), sprSeconds(tft),
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
  lastTime = "";
  lastDate = "";
  lastDayOfWeek = "";
  lastTemp = -999;
  lastHumidity = -999;
  lastTOD = "";
  lastSeconds = "";
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
  updateAlarmSprite();
  drawSeconds(tft);
  drawTemperature(tft);
  drawHumidity(tft);
  drawClock(tft);
  drawDate(tft);
  drawDayOfWeek(tft);
}

void ClockPage::setupLayout(TFT_eSPI &tft)
{
  int screenWidth = tft.width();
  int screenHeight = tft.height();

  // --- Clock Block Layout ---
  clockY = MARGIN;

  // Calculate the width of the side elements column (AM/PM and seconds)
  int sideWidth = max(sprTOD.width(), sprSeconds.width());
  int timeSideGap = 15; // Gap between HH:MM and the side elements

  // Calculate the total width of the entire clock display
  int totalWidth = sprClock.width() + timeSideGap + sideWidth;

  // Calculate the starting X position to center the clock display
  int startX = (screenWidth - totalWidth) / 2;
  if (startX < 0)
    startX = 0;

  // Set the position for the main clock sprite
  clockX = startX - 15;

  // Calculate the starting X for the side elements
  sideX = clockX + sprClock.width() + timeSideGap;

  // Calculate the total height of the side elements stack
  int sideElementsVGap = 15;
  int sideElementsHeight = sprTOD.height() + sideElementsVGap + sprSeconds.height();

  // Calculate the starting Y to vertically center the side elements against the main clock
  int sideStartY = clockY + (sprClock.height() - sideElementsHeight) / 2;
  if (sideStartY < clockY)
    sideStartY = clockY;

  // Set positions for AM/PM and Seconds sprites
  todX = sideX + sideElementsVGap;
  todY = sideStartY;
  secondsX = todX + (sprTOD.width() - sprSeconds.width()) / 2;
  secondsY = todY + sprTOD.height() + sideElementsVGap + 3;

  // --- Bottom Rows Layout (Date and Sensors) ---
  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();
  dateY = screenHeight - (fontHeight * 2 + MARGIN + 40);
  sensorY = screenHeight - (fontHeight + MARGIN + 10);

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
  sprClock.createSprite(343, 106);
  sprClock.loadFont(DSEG7ModernBold104);
  sprClock.setTextDatum(MR_DATUM);

  sprTOD.createSprite(45, 34);
  sprTOD.loadFont(DSEG14ModernBold32);
  sprTOD.setTextDatum(TR_DATUM);

  sprSeconds.createSprite(70, 50);
  sprSeconds.loadFont(DSEG7ModernBold48);
  sprSeconds.setTextDatum(TR_DATUM);

  sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDayOfWeek.loadFont(DSEG14ModernBold48);
  sprDayOfWeek.setTextDatum(ML_DATUM);

  sprDate.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDate.loadFont(DSEG14ModernBold48);
  sprDate.setTextDatum(MR_DATUM);

  sprTemp.createSprite(tft.width() / 2 - MARGIN, 50);
  sprTemp.loadFont(DSEG14ModernBold48);
  sprTemp.setTextDatum(ML_DATUM);

  sprHumidity.createSprite(tft.width() / 2 - MARGIN, 50);
  sprHumidity.loadFont(DSEG14ModernBold48);
  sprHumidity.setTextDatum(MR_DATUM);

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

  sprClock.setTextColor(timeColor, _bgColor);
  sprTOD.setTextColor(todColor, _bgColor);
  sprSeconds.setTextColor(secondsColor, _bgColor);
  sprDayOfWeek.setTextColor(dayOfWeekColor, _bgColor);
  sprDate.setTextColor(dateColor, _bgColor);
  sprTemp.setTextColor(tempColor, _bgColor);
  sprHumidity.setTextColor(humidityColor, _bgColor);
}

void ClockPage::drawClock(TFT_eSPI &tft)
{
  auto &timeManager = TimeManager::getInstance();
  bool is24Hour = timeManager.is24HourFormat();
  String timeStr = timeManager.getFormattedTime();
  String todStr = timeManager.getTOD();

  // Draw the main time (HH:MM) if it has changed
  if (timeStr != lastTime)
  {
    sprClock.fillSprite(_bgColor);
    sprClock.drawString(timeStr.c_str(), sprClock.width(), sprClock.height() / 2);
#ifdef DEBUG_BORDERS
    sprClock.drawRect(0, 0, sprClock.width(), sprClock.height(), TFT_RED);
#endif
    sprClock.pushSprite(clockX, clockY);
    lastTime = timeStr;
  }

  // Handle drawing or clearing the AM/PM sprite
  if (!is24Hour)
  {
    // In 12-hour mode, draw the TOD if it has changed
    if (todStr != lastTOD)
    {
      sprTOD.fillSprite(_bgColor);
      sprTOD.drawString(todStr.c_str(), sprTOD.width(), 0);
#ifdef DEBUG_BORDERS
      sprTOD.drawRect(0, 0, sprTOD.width(), sprTOD.height(), TFT_GREEN);
#endif
      sprTOD.pushSprite(todX, todY);
      lastTOD = todStr;
    }
  }
}

void ClockPage::drawSeconds(TFT_eSPI &tft)
{
  String secondsStr = TimeManager::getInstance().getFormattedSeconds();
  if (secondsStr == lastSeconds)
  {
    return; // No change
  }

  sprSeconds.fillSprite(_bgColor);
  sprSeconds.drawString(secondsStr.c_str(), sprSeconds.width(), 0);
#ifdef DEBUG_BORDERS
  sprSeconds.drawRect(0, 0, sprSeconds.width(), sprSeconds.height(), TFT_MAGENTA);
#endif
  // Position the seconds sprite under the AM/PM sprite's location
  sprSeconds.pushSprite(secondsX, secondsY);
  lastSeconds = secondsStr;
}

void ClockPage::drawDayOfWeek(TFT_eSPI &tft)
{
  String dayStr = TimeManager::getInstance().getDayOfWeek();
  if (dayStr != lastDayOfWeek)
  {
    sprDayOfWeek.fillSprite(_bgColor);
    sprDayOfWeek.drawString(dayStr.c_str(), 0, sprDayOfWeek.height() / 2);

#ifdef DEBUG_BORDERS
    sprDayOfWeek.drawRect(0, 0, sprDayOfWeek.width(), sprDayOfWeek.height(), TFT_BLUE);
#endif

    sprDayOfWeek.pushSprite(MARGIN, dateY);
    lastDayOfWeek = dayStr;
  }
}

void ClockPage::drawDate(TFT_eSPI &tft)
{
  String dateStr = TimeManager::getInstance().getFormattedDate();
  if (dateStr != lastDate)
  {
    sprDate.fillSprite(_bgColor);
    sprDate.drawString(dateStr.c_str(), sprDate.width(), sprDate.height() / 2);

#ifdef DEBUG_BORDERS
    sprDate.drawRect(0, 0, sprDate.width(), sprDate.height(), TFT_YELLOW);
#endif

    sprDate.pushSprite(tft.width() / 2, dateY);
    lastDate = dateStr;
  }
}

void ClockPage::drawTemperature(TFT_eSPI &tft)
{
  float temp = getTemperature(); // Use the cached value
  if (fabs(temp - lastTemp) < 0.1)
    return;

  auto &config = ConfigManager::getInstance();
  uint16_t tempColor = hexToRGB565(config.getTempColor());

  sprTemp.fillSprite(_bgColor); // Clear sprite

  // Set the font for the temperature value
  sprTemp.loadFont(DSEG14ModernBold48);

  // Draw temperature value
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
  sprTemp.drawString(tempBuf, 0, sprTemp.height() / 2);

  // --- Draw degree symbol ---
  int tempWidth = sprTemp.textWidth(tempBuf);
  int fontHeight = sprTemp.fontHeight();

  // Scale the circle's size and position based on the font
  int circleRadius = max(2, fontHeight / 14);
  int circleX = tempWidth + circleRadius + 2;
  // Position the circle near the top of the temperature digits
  int circleY = (sprTemp.height() / 2) - (fontHeight / 2) + circleRadius;
  sprTemp.fillCircle(circleX, circleY, circleRadius, tempColor);

  // --- Draw the unit (C/F) ---
  sprTemp.loadFont(DSEG14ModernBold32);
  sprTemp.setTextDatum(TL_DATUM); // Set datum to Top-Left for correct alignment
  char unit = config.isCelsius() ? 'C' : 'F';
  char unitBuf[2] = {unit, '\0'};

  // Position unit after the degree symbol, aligning its top with the temperature's top
  int unitX = circleX + circleRadius + 2;
  int unitY = (sprTemp.height() / 2) - (fontHeight / 2);
  sprTemp.drawString(unitBuf, unitX, unitY);

  sprTemp.setTextDatum(ML_DATUM); // Restore the original datum

#ifdef DEBUG_BORDERS
  sprTemp.drawRect(0, 0, sprTemp.width(), sprTemp.height(), TFT_ORANGE);
#endif

  sprTemp.pushSprite(MARGIN, sensorY);
  lastTemp = temp;
}

void ClockPage::drawHumidity(TFT_eSPI &tft)
{
  float humidity = getHumidity(); // Use the cached value
  if (fabs(humidity - lastHumidity) < 0.1)
    return;

  char buf[24];
  if (humidity < 0)
  {
    snprintf(buf, sizeof(buf), "N/A");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%.0f%%", humidity);
  }

  sprHumidity.fillSprite(_bgColor);
  sprHumidity.drawString(buf, sprHumidity.width(), sprHumidity.height() / 2);

#ifdef DEBUG_BORDERS
  sprHumidity.drawRect(0, 0, sprHumidity.width(), sprHumidity.height(), TFT_CYAN);
#endif

  sprHumidity.pushSprite(tft.width() / 2, sensorY);
  lastHumidity = humidity;
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
    sprTOD.fillSprite(_bgColor);
    sprTOD.pushSprite(todX, todY);
  }

  // Force a redraw of all elements by resetting their last known values
  lastTime = "";
  lastDate = "";
  lastDayOfWeek = "";
  lastTemp = -999;
  lastHumidity = -999;
  lastTOD = "";
  lastSeconds = "";
}

void ClockPage::initAlarmSprite(TFT_eSPI &tft)
{
  tft.loadFont(CenturyGothicBold48);
  int textWidth = tft.textWidth("ALARM");
  _alarmSprite.createSprite(textWidth + 20, 50);
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
      _alarmSprite.fillRect(0, _alarmSprite.height() - 10, barWidth, 10, hexToRGB565(config.getAlarmTextColor().c_str()));
    }
  }
  else if (anySnoozed)
  {
    _dismissProgress = 0.0f;
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
        break;
      }
    }
  }
  else
  {
    _dismissProgress = 0.0f;
  }

  _alarmSprite.pushSprite(_alarmSpriteX, _alarmSpriteY);
}
