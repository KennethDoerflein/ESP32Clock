#include "display.h"
#include "DSEG14ModernBold40.h"
#include "DSEG14ModernBold24.h"

void Display::begin()
{
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  setupSprites();
}

void Display::setupSprites()
{
  // Setup clock sprite
  sprClock.createSprite(tft.width(), 80);
  sprClock.loadFont(DSEG14ModernBold40);
  sprClock.setTextDatum(MC_DATUM);
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  // Setup day sprite
  sprDay.createSprite(tft.width(), 30);
  sprDay.loadFont(DSEG14ModernBold24);
  sprDay.setTextDatum(MC_DATUM);
  sprDay.setTextColor(TFT_ORANGE, TFT_BLACK);

  // Setup date sprite
  sprDate.createSprite(tft.width(), 30);
  sprDate.loadFont(DSEG14ModernBold24);
  sprDate.setTextDatum(MC_DATUM);
  sprDate.setTextColor(TFT_YELLOW, TFT_BLACK);

  // Setup sensor sprite
  sprSensor.createSprite(tft.width(), 40);
  sprSensor.loadFont(DSEG14ModernBold24);
  sprSensor.setTextDatum(MC_DATUM);
  sprSensor.setTextColor(TFT_GREEN, TFT_BLACK);
}

void Display::drawClock(const char *timeStr)
{
  sprClock.fillSprite(TFT_BLACK);
  sprClock.drawString(timeStr, sprClock.width() / 2, sprClock.height() / 2);
  sprClock.pushSprite(0, 0);
}

void Display::drawDayOfWeek(const char *dayStr)
{
  sprDay.fillSprite(TFT_BLACK);
  sprDay.drawString(dayStr, sprDay.width() / 2, sprDay.height() / 2);
  sprDay.pushSprite(0, 80); // Position it below the clock
}

void Display::drawDate(const char *dateStr)
{
  sprDate.fillSprite(TFT_BLACK);
  sprDate.drawString(dateStr, sprDate.width() / 2, sprDate.height() / 2);
  sprDate.pushSprite(0, 110); // below the day of the week
}

void Display::drawSensors(float temp, float humidity)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1fC  %.0f%%", temp, humidity);

  sprSensor.fillSprite(TFT_BLACK);
  sprSensor.drawString(buf, sprSensor.width() / 2, sprSensor.height() / 2);
  sprSensor.pushSprite(0, 150); // below the date
}
