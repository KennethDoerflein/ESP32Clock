#include "display.h"
#include "DSEG14ModernBold40.h"
#include "DSEG14ModernBold24.h"

// Global display objects
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprClock = TFT_eSprite(&tft);
TFT_eSprite sprDate = TFT_eSprite(&tft);
TFT_eSprite sprSensor = TFT_eSprite(&tft);

void setupDisplay()
{
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Create sprites
  sprClock.createSprite(tft.width(), 80);
  sprClock.loadFont(DSEG14ModernBold40);
  sprClock.setTextDatum(MC_DATUM);
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  sprDate.createSprite(tft.width(), 30);
  sprDate.loadFont(DSEG14ModernBold24);
  sprDate.setTextDatum(MC_DATUM);
  sprDate.setTextColor(TFT_YELLOW, TFT_BLACK);

  sprSensor.createSprite(tft.width(), 40);
  sprSensor.loadFont(DSEG14ModernBold24);
  sprSensor.setTextDatum(MC_DATUM);
  sprSensor.setTextColor(TFT_GREEN, TFT_BLACK);
}

void drawClock(const char *timeStr)
{
  sprClock.fillSprite(TFT_BLACK);
  sprClock.drawString(timeStr, sprClock.width() / 2, sprClock.height() / 2);
  sprClock.pushSprite(0, 0);
}

void drawDate(const char *dateStr)
{
  sprDate.fillSprite(TFT_BLACK);
  sprDate.drawString(dateStr, sprDate.width() / 2, sprDate.height() / 2);
  sprDate.pushSprite(0, 85); // below clock
}

void drawSensors(float temp, float humidity)
{
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1fC  %.0f%%", temp, humidity);

  sprSensor.fillSprite(TFT_BLACK);
  sprSensor.drawString(buf, sprSensor.width() / 2, sprSensor.height() / 2);
  sprSensor.pushSprite(0, 120); // below date
}
