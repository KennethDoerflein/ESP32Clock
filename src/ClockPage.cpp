#include "pages/ClockPage.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "sensors.h"
#include "fonts/DSEG7ModernBold72.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold16.h"
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
  dateY = screenHeight - (fontHeight * 2 + MARGIN + 15);
  sensorY = screenHeight - (fontHeight + MARGIN + 5);
}

void ClockPage::setupSprites(TFT_eSPI &tft)
{
  // Create the sprites and configure them
  sprClock.createSprite(280, 90);
  sprClock.loadFont(DSEG7ModernBold72);
  sprClock.setTextDatum(MC_DATUM);
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprTOD.createSprite(35, 50);
  sprTOD.loadFont(DSEG14ModernBold16);
  sprTOD.setTextDatum(TR_DATUM);
  sprTOD.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDayOfWeek.loadFont(DSEG14ModernBold32);
  sprDayOfWeek.setTextDatum(ML_DATUM);
  sprDayOfWeek.setTextColor(TFT_WHITE, TFT_BLACK);

  sprDate.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDate.loadFont(DSEG14ModernBold32);
  sprDate.setTextDatum(MR_DATUM);
  sprDate.setTextColor(TFT_WHITE, TFT_BLACK);

  sprTemp.createSprite(tft.width() / 2 - MARGIN, 50);
  sprTemp.loadFont(DSEG14ModernBold32);
  sprTemp.setTextDatum(ML_DATUM);
  sprTemp.setTextColor(TFT_WHITE, TFT_BLACK);

  sprHumidity.createSprite(tft.width() / 2 - MARGIN, 50);
  sprHumidity.loadFont(DSEG14ModernBold32);
  sprHumidity.setTextDatum(MR_DATUM);
  sprHumidity.setTextColor(TFT_WHITE, TFT_BLACK);
}

void ClockPage::drawClock(TFT_eSPI &tft)
{
  auto &timeManager = TimeManager::getInstance();
  String timeStr = timeManager.getFormattedTime();
  String todStr = timeManager.getTOD();

  if (timeStr == lastTime && todStr == lastTOD)
    return;

  sprClock.fillSprite(TFT_BLACK);
  sprClock.drawString(timeStr.c_str(), sprClock.width() / 2, sprClock.height() / 2);

  bool is24Hour = (todStr.length() == 0);
  int clockX = (tft.width() - (is24Hour ? sprClock.width() : sprClock.width() + sprTOD.width())) / 2 - 20;
  if (clockX < 0)
    clockX = 0;

  sprClock.pushSprite(clockX, clockY);
  lastTime = timeStr;

  if (todStr != lastTOD)
  {
    sprTOD.fillSprite(TFT_BLACK);
    sprTOD.drawString(todStr.c_str(), sprTOD.width(), 0);
    sprTOD.pushSprite(clockX + sprClock.width(), clockY + 10);
    lastTOD = todStr;
  }
}

void ClockPage::drawDayOfWeek(TFT_eSPI &tft)
{
  String dayStr = TimeManager::getInstance().getDayOfWeek();
  if (dayStr != lastDayOfWeek)
  {
    sprDayOfWeek.fillSprite(TFT_BLACK);
    sprDayOfWeek.drawString(dayStr.c_str(), 0, sprDayOfWeek.height() / 2);
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
  sprHumidity.pushSprite(tft.width() / 2, sensorY);
  lastHumidity = humidity;
}