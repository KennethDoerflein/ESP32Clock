// Comment out the next line to disable sprite borders for debugging
// #define DEBUG_BORDERS

#include "pages/ClockPage.h"
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
  tft.fillScreen(TFT_BLACK);
  setupLayout(tft);
  if (!_spritesCreated)
  {
    setupSprites(tft);
    _spritesCreated = true;
  }

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
  drawClock(tft);
  drawDate(tft);
  drawDayOfWeek(tft);
  drawTemperature(tft);
  drawHumidity(tft);
}

void ClockPage::setupLayout(TFT_eSPI &tft)
{
  int screenHeight = tft.height();
  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();

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
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprTOD.createSprite(45, 34);
  sprTOD.loadFont(DSEG14ModernBold32);
  sprTOD.setTextDatum(TR_DATUM);
  sprTOD.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprSeconds.createSprite(70, 50);
  sprSeconds.loadFont(DSEG7ModernBold48);
  sprSeconds.setTextDatum(TR_DATUM);
  sprSeconds.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDayOfWeek.loadFont(DSEG14ModernBold48);
  sprDayOfWeek.setTextDatum(ML_DATUM);
  sprDayOfWeek.setTextColor(TFT_WHITE, TFT_BLACK);

  sprDate.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDate.loadFont(DSEG14ModernBold48);
  sprDate.setTextDatum(MR_DATUM);
  sprDate.setTextColor(TFT_WHITE, TFT_BLACK);

  sprTemp.createSprite(tft.width() / 2 - MARGIN, 50);
  sprTemp.loadFont(DSEG14ModernBold48);
  sprTemp.setTextDatum(ML_DATUM);
  sprTemp.setTextColor(TFT_WHITE, TFT_BLACK);

  sprHumidity.createSprite(tft.width() / 2 - MARGIN, 50);
  sprHumidity.loadFont(DSEG14ModernBold48);
  sprHumidity.setTextDatum(MR_DATUM);
  sprHumidity.setTextColor(TFT_WHITE, TFT_BLACK);
}

void ClockPage::drawClock(TFT_eSPI &tft)
{
  auto &timeManager = TimeManager::getInstance();
  bool is24Hour = timeManager.is24HourFormat();
  String timeStr = timeManager.getFormattedTime();
  String todStr = timeManager.getTOD();
  String secondsStr = timeManager.getFormattedSeconds();
  int clockX = (tft.width() - (sprClock.width() + sprTOD.width())) / 2 - 40;
  if (clockX < 0)
    clockX = 0;

  int secondsX = clockX + sprClock.width() + 15;
  int todX = secondsX + sprSeconds.width() * 0.2;

  // Draw the main time (HH:MM) if it has changed
  if (timeStr != lastTime)
  {
    sprClock.fillSprite(TFT_BLACK);
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
      sprTOD.fillSprite(TFT_BLACK);
      sprTOD.drawString(todStr.c_str(), sprTOD.width(), 0);
#ifdef DEBUG_BORDERS
      sprTOD.drawRect(0, 0, sprTOD.width(), sprTOD.height(), TFT_GREEN);
#endif
      sprTOD.pushSprite(todX, clockY);
      lastTOD = todStr;
    }
  }
  else
  {
    // In 24-hour mode, clear the TOD if it was previously displayed
    if (lastTOD != "")
    {
      sprTOD.fillSprite(TFT_BLACK);
      sprTOD.pushSprite(todX, clockY + 10);
      lastTOD = ""; // Mark as cleared
    }
  }

  // Draw the seconds if they have changed
  if (secondsStr != lastSeconds)
  {
    sprSeconds.fillSprite(TFT_BLACK);
    sprSeconds.drawString(secondsStr.c_str(), sprSeconds.width(), 0);
#ifdef DEBUG_BORDERS
    sprSeconds.drawRect(0, 0, sprSeconds.width(), sprSeconds.height(), TFT_MAGENTA);
#endif
    // Position the seconds sprite under the AM/PM sprite's location
    sprSeconds.pushSprite(secondsX, clockY + 20 + sprTOD.height());
    lastSeconds = secondsStr;
  }
}

void ClockPage::drawDayOfWeek(TFT_eSPI &tft)
{
  String dayStr = TimeManager::getInstance().getDayOfWeek();
  if (dayStr != lastDayOfWeek)
  {
    sprDayOfWeek.fillSprite(TFT_BLACK);
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
    sprDate.fillSprite(TFT_BLACK);
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
  if (abs(temp - lastTemp) < 0.1)
    return;

  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f%c", temp, ConfigManager::getInstance().isCelsius() ? 'C' : 'F');
  sprTemp.fillSprite(TFT_BLACK);
  sprTemp.drawString(buf, 0, sprTemp.height() / 2);

#ifdef DEBUG_BORDERS
  sprTemp.drawRect(0, 0, sprTemp.width(), sprTemp.height(), TFT_ORANGE);
#endif

  sprTemp.pushSprite(MARGIN, sensorY);
  lastTemp = temp;
}

void ClockPage::drawHumidity(TFT_eSPI &tft)
{
  float humidity = getHumidity(); // Use the cached value
  if (abs(humidity - lastHumidity) < 0.1)
    return;

  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f%%", humidity);
  sprHumidity.fillSprite(TFT_BLACK);
  sprHumidity.drawString(buf, sprHumidity.width(), sprHumidity.height() / 2);

#ifdef DEBUG_BORDERS
  sprHumidity.drawRect(0, 0, sprHumidity.width(), sprHumidity.height(), TFT_CYAN);
#endif

  sprHumidity.pushSprite(tft.width() / 2, sensorY);
  lastHumidity = humidity;
}

void ClockPage::refresh()
{
  // Force a redraw of all elements by resetting their last known values
  lastTime = "";
  lastDate = "";
  lastDayOfWeek = "";
  lastTemp = -999;
  lastHumidity = -999;
  // lastTOD is not reset here, to allow the logic in drawClock
  // to correctly clear the sprite when switching from 12 to 24-hour mode.
}