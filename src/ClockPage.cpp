// Comment out the next line to disable sprite borders for debugging
// #define DEBUG_BORDERS

#include "pages/ClockPage.h"
#include "utils.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "sensors.h"
#include "fonts/DSEG7ModernBold104.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold48.h"
#include "fonts/DSEG7ModernBold48.h"

#include <Arduino.h>

#define MARGIN 10

ClockPage::ClockPage(TFT_eSPI *tft)
    : sprClock(tft), sprDayOfWeek(tft), sprDate(tft), sprTemp(tft), sprHumidity(tft), sprTOD(tft), sprSeconds(tft)
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
  drawSeconds(tft);
  drawTemperature(tft);
  drawHumidity(tft);
  drawClock(tft);
  drawDate(tft);
  drawDayOfWeek(tft);
}

void ClockPage::setupLayout(TFT_eSPI &tft)
{
  int screenHeight = tft.height();
  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();

  clockX = (tft.width() - (sprClock.width() + sprTOD.width())) / 2 - 40;
  if (clockX < 0)
    clockX = 0;

  clockY = MARGIN;
  dateY = screenHeight - (fontHeight * 2 + MARGIN + 40);
  sensorY = screenHeight - (fontHeight + MARGIN + 10);
}

void ClockPage::setupSprites(TFT_eSPI &tft)
{
  // Create the sprites and configure them
  sprClock.createSprite(350, 106);
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

  int secondsX = clockX + sprClock.width() + 15;
  int todX = secondsX + sprSeconds.width() * 0.15;

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
      sprTOD.pushSprite(todX, clockY);
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

  int secondsX = clockX + sprClock.width() + 15;

  sprSeconds.fillSprite(_bgColor);
  sprSeconds.drawString(secondsStr.c_str(), sprSeconds.width(), 0);
#ifdef DEBUG_BORDERS
  sprSeconds.drawRect(0, 0, sprSeconds.width(), sprSeconds.height(), TFT_MAGENTA);
#endif
  // Position the seconds sprite under the AM/PM sprite's location
  sprSeconds.pushSprite(secondsX, clockY + 20 + sprTOD.height());
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

  // Draw temperature value
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
  sprTemp.drawString(tempBuf, 0, sprTemp.height() / 2);

  // Calculate position and draw degree circle
  int tempWidth = sprTemp.textWidth(tempBuf);
  int circleX = tempWidth + 4;
  int circleY = sprTemp.height() / 2 - 20;
  int circleRadius = 3;
  sprTemp.fillCircle(circleX, circleY, circleRadius, tempColor);

  // Draw the unit (C/F)
  char unit = config.isCelsius() ? 'C' : 'F';
  char unitBuf[2] = {unit, '\0'};
  int unitX = circleX + circleRadius * 2; // Position it after the circle
  sprTemp.drawString(unitBuf, unitX, sprTemp.height() / 2);

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
    int secondsX = clockX + sprClock.width() + 15;
    int todX = secondsX + sprSeconds.width() * 0.15;
    sprTOD.pushSprite(todX, clockY);
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
