// display.cpp

#include "display.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "fonts/DSEG7ModernBold72.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold16.h"
#include <Arduino.h>

#define MARGIN 10

// LEDC constants for backlight control
#define BACKLIGHT_CHANNEL 0
#define BACKLIGHT_FREQ 5000
#define BACKLIGHT_RESOLUTION 8

void Display::begin()
{
  // Setup backlight PWM
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
  ledcAttachPin(TFT_BL, BACKLIGHT_CHANNEL);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  setupLayout();
  setupSprites();
}

void Display::setupLayout()
{
  int screenHeight = tft.height();

  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();

  clockY = MARGIN;
  dateY = screenHeight - (fontHeight * 2 + MARGIN + 15);
  sensorY = screenHeight - (fontHeight + MARGIN + 5);
}

void Display::drawLayout()
{
  tft.fillScreen(TFT_BLACK);
}

void Display::setupSprites()
{
  // Setup clock sprite
  sprClock.createSprite(280, 90);
  sprClock.loadFont(DSEG7ModernBold72);
  sprClock.setTextDatum(MC_DATUM);
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  // Setup day sprite
  sprTOD.createSprite(35, 50);
  sprTOD.loadFont(DSEG14ModernBold16);
  sprTOD.setTextDatum(TR_DATUM);
  sprTOD.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  // Setup day of week sprite
  sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDayOfWeek.loadFont(DSEG14ModernBold32);
  sprDayOfWeek.setTextDatum(ML_DATUM);
  sprDayOfWeek.setTextColor(TFT_WHITE, TFT_BLACK);

  // Setup date sprite
  sprDate.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDate.loadFont(DSEG14ModernBold32);
  sprDate.setTextDatum(MR_DATUM);
  sprDate.setTextColor(TFT_WHITE, TFT_BLACK);

  // Setup temp sprite
  sprTemp.createSprite(tft.width() / 2 - MARGIN, 50);
  sprTemp.loadFont(DSEG14ModernBold32);
  sprTemp.setTextDatum(ML_DATUM);
  sprTemp.setTextColor(TFT_WHITE, TFT_BLACK);

  // Setup humidity sprite
  sprHumidity.createSprite(tft.width() / 2 - MARGIN, 50);
  sprHumidity.loadFont(DSEG14ModernBold32);
  sprHumidity.setTextDatum(MR_DATUM);
  sprHumidity.setTextColor(TFT_WHITE, TFT_BLACK);
}

void Display::drawClock(const char *timeStr, const char *todStr)
{
  if (String(timeStr) == lastTime && String(todStr) == lastTOD)
  {
    return; // No change, no need to redraw
  }

  sprClock.fillSprite(TFT_BLACK);
  sprClock.drawString(timeStr, sprClock.width() / 2, sprClock.height() / 2);

  bool is24Hour = (strlen(todStr) == 0);
  int clockX;

  if (is24Hour)
  {
    clockX = (tft.width() - sprClock.width()) / 2;
  }
  else
  {
    int totalWidth = sprClock.width() + sprTOD.width();
    clockX = (tft.width() - totalWidth) / 2;
  }

  clockX -= 20;
  if (clockX < 0)
    clockX = 0;

  sprClock.pushSprite(clockX, clockY);
  lastTime = timeStr;

  if (String(todStr) != lastTOD)
  {
    sprTOD.fillSprite(TFT_BLACK);
    sprTOD.drawString(todStr, sprTOD.width(), 0);
    int todX = clockX + sprClock.width();
    sprTOD.pushSprite(todX, clockY + 10);
    lastTOD = todStr;
  }
}

void Display::updateSprite(TFT_eSprite &sprite, const char *text, int x, int y)
{
  sprite.fillSprite(TFT_BLACK);

  if (sprite.getTextDatum() == ML_DATUM)
  {
    sprite.drawString(text, 0, sprite.height() / 2);
  }
  else if (sprite.getTextDatum() == MR_DATUM)
  {
    sprite.drawString(text, sprite.width(), sprite.height() / 2);
  }
  else
  {
    sprite.drawString(text, sprite.width() / 2, sprite.height() / 2);
  }

  sprite.pushSprite(x, y);
}

void Display::drawDayOfWeek(const char *dayStr)
{
  if (String(dayStr) != lastDayOfWeek)
  {
    updateSprite(sprDayOfWeek, dayStr, MARGIN, dateY);
    lastDayOfWeek = dayStr;
  }
}

void Display::drawDate(const char *dateStr)
{
  if (String(dateStr) != lastDate)
  {
    updateSprite(sprDate, dateStr, tft.width() / 2, dateY);
    lastDate = dateStr;
  }
}

void Display::drawTemperature(float temp, bool isCelsius)
{
  char buf[16];
  char unit = isCelsius ? 'C' : 'F';
  snprintf(buf, sizeof(buf), "%.0f%c", temp, unit);
  updateSprite(sprTemp, buf, MARGIN, sensorY);
}

void Display::drawHumidity(float humidity)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f%%", humidity);
  updateSprite(sprHumidity, buf, tft.width() / 2, sensorY);
}

void Display::drawStatusMessage(const char *message)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(DSEG14ModernBold16);
  tft.drawString(message, tft.width() / 2, tft.height() / 2);
}

void Display::updateBrightness()
{
  auto &config = ConfigManager::getInstance();
  int dutyCycle;

  if (config.isAutoBrightness())
  {
    uint8_t hour = TimeManager::getInstance().getHour();
    // Full brightness from 7 AM to 9 PM (21:00)
    if (hour >= 7 && hour < 21)
    {
      dutyCycle = 255; // Max brightness
    }
    else
    {
      dutyCycle = 85; // Dim brightness (about 33%)
    }
  }
  else
  {
    dutyCycle = config.getBrightness();
  }
  ledcWrite(BACKLIGHT_CHANNEL, dutyCycle);
}