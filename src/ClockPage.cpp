// Comment out the next line to disable sprite borders for debugging
// #define DEBUG_BORDERS

#include "pages/ClockPage.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "sensors.h"
#include "fonts/DSEG7ModernBold104.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold48.h"
#include <Arduino.h>

#define MARGIN 10

ClockPage::ClockPage(TFT_eSPI *tft)
    : sprClock(tft), sprDayOfWeek(tft), sprDate(tft), sprTemp(tft), sprHumidity(tft), sprTOD(tft)
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
  sprClock.setTextDatum(MC_DATUM);
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprTOD.createSprite(55, 34);
  sprTOD.loadFont(DSEG14ModernBold32);
  sprTOD.setTextDatum(TR_DATUM);
  sprTOD.setTextColor(TFT_SKYBLUE, TFT_BLACK);

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
#ifdef DEBUG_BORDERS
  String timeStr = "88:88";
#else
  String timeStr = timeManager.getFormattedTime();
#endif
  String todStr = timeManager.getTOD();

  if (timeStr == lastTime && todStr == lastTOD)
    return;

  sprClock.fillSprite(TFT_BLACK);
  sprClock.drawString(timeStr.c_str(), sprClock.width() / 2, sprClock.height() / 2);

#ifdef DEBUG_BORDERS
  sprClock.drawRect(0, 0, sprClock.width(), sprClock.height(), TFT_RED);
#endif

  bool is24Hour = timeManager.is24HourFormat();
  int clockX = (tft.width() - (sprClock.width() + sprTOD.width())) / 2 - 40;
  if (clockX < 0)
    clockX = 0;

  sprClock.pushSprite(clockX, clockY);
  lastTime = timeStr;

  if (!is24Hour && todStr != lastTOD)
  {
    sprTOD.fillSprite(TFT_BLACK);
    sprTOD.drawString(todStr.c_str(), sprTOD.width(), 0);

#ifdef DEBUG_BORDERS
    sprTOD.drawRect(0, 0, sprTOD.width(), sprTOD.height(), TFT_GREEN);
#endif

    lastTOD = todStr;
  }
  else
  {
    sprTOD.fillSprite(TFT_BLACK);
    lastTOD = "";
  }

  sprTOD.pushSprite(clockX + sprClock.width() + 50, clockY + 10);
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
  lastTOD = "";
}