// display.cpp

#include "display.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "fonts/DSEG7ModernBold72.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold16.h"
#include <Arduino.h>

// Defines the margin for UI elements from the edge of the screen.
#define MARGIN 10

// LEDC (LED Control) constants for managing the backlight PWM.
#define BACKLIGHT_CHANNEL 0    // LEDC channel 0
#define BACKLIGHT_FREQ 5000    // 5 kHz frequency
#define BACKLIGHT_RESOLUTION 8 // 8-bit resolution (0-255)

void Display::begin()
{
  // Configure the LEDC peripheral for PWM control of the backlight.
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
  // Attach the TFT_BL pin to the configured LEDC channel.
  ledcAttachPin(TFT_BL, BACKLIGHT_CHANNEL);

  // Initialize the TFT display driver.
  tft.init();
  tft.setRotation(3); // Set screen rotation (adjust as needed).
  tft.fillScreen(TFT_BLACK);

  // Calculate initial layout positions and set up sprites.
  setupLayout();
  setupSprites();
}

void Display::setupLayout()
{
  int screenHeight = tft.height();

  // Load a font to measure its height for layout calculations.
  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();

  // Calculate the Y positions for the main UI sections.
  clockY = MARGIN;
  dateY = screenHeight - (fontHeight * 2 + MARGIN + 15);
  sensorY = screenHeight - (fontHeight + MARGIN + 5);
}

void Display::drawLayout()
{
  // Simply clears the screen. Can be expanded to draw static layout elements.
  tft.fillScreen(TFT_BLACK);
}

void Display::setupSprites()
{
  // Create and configure the sprite for the main clock time.
  sprClock.createSprite(280, 90);
  sprClock.loadFont(DSEG7ModernBold72);
  sprClock.setTextDatum(MC_DATUM); // Middle-Center datum
  sprClock.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  // Create and configure the sprite for AM/PM indicator.
  sprTOD.createSprite(35, 50);
  sprTOD.loadFont(DSEG14ModernBold16);
  sprTOD.setTextDatum(TR_DATUM); // Top-Right datum
  sprTOD.setTextColor(TFT_SKYBLUE, TFT_BLACK);

  // Create and configure the sprite for the day of the week.
  sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDayOfWeek.loadFont(DSEG14ModernBold32);
  sprDayOfWeek.setTextDatum(ML_DATUM); // Middle-Left datum
  sprDayOfWeek.setTextColor(TFT_WHITE, TFT_BLACK);

  // Create and configure the sprite for the date.
  sprDate.createSprite(tft.width() / 2 - MARGIN, 50);
  sprDate.loadFont(DSEG14ModernBold32);
  sprDate.setTextDatum(MR_DATUM); // Middle-Right datum
  sprDate.setTextColor(TFT_WHITE, TFT_BLACK);

  // Create and configure the sprite for temperature.
  sprTemp.createSprite(tft.width() / 2 - MARGIN, 50);
  sprTemp.loadFont(DSEG14ModernBold32);
  sprTemp.setTextDatum(ML_DATUM);
  sprTemp.setTextColor(TFT_WHITE, TFT_BLACK);

  // Create and configure the sprite for humidity.
  sprHumidity.createSprite(tft.width() / 2 - MARGIN, 50);
  sprHumidity.loadFont(DSEG14ModernBold32);
  sprHumidity.setTextDatum(MR_DATUM);
  sprHumidity.setTextColor(TFT_WHITE, TFT_BLACK);
}

void Display::drawClock(const char *timeStr, const char *todStr)
{
  // Only redraw if the time or AM/PM has changed, to save CPU and prevent flicker.
  if (String(timeStr) == lastTime && String(todStr) == lastTOD)
  {
    return;
  }

  // Draw the main time string onto its sprite.
  sprClock.fillSprite(TFT_BLACK);
  sprClock.drawString(timeStr, sprClock.width() / 2, sprClock.height() / 2);

  // Calculate the horizontal position to center the clock display.
  bool is24Hour = (strlen(todStr) == 0);
  int clockX;
  if (is24Hour)
  {
    // Center the clock sprite alone.
    clockX = (tft.width() - sprClock.width()) / 2;
  }
  else
  {
    // Center the combined width of the clock and AM/PM sprites.
    int totalWidth = sprClock.width() + sprTOD.width();
    clockX = (tft.width() - totalWidth) / 2;
  }

  clockX -= 20; // Fine-tune position.
  if (clockX < 0)
    clockX = 0;

  // Push the updated clock sprite to the screen.
  sprClock.pushSprite(clockX, clockY);
  lastTime = timeStr; // Cache the new time.

  // Redraw the AM/PM sprite only if it has changed.
  if (String(todStr) != lastTOD)
  {
    sprTOD.fillSprite(TFT_BLACK);
    sprTOD.drawString(todStr, sprTOD.width(), 0);
    int todX = clockX + sprClock.width();
    sprTOD.pushSprite(todX, clockY + 10);
    lastTOD = todStr; // Cache the new AM/PM status.
  }
}

void Display::updateSprite(TFT_eSprite &sprite, const char *text, int x, int y)
{
  // This is a generic helper to draw text onto a sprite and push it to the screen.
  sprite.fillSprite(TFT_BLACK); // Clear the sprite first.

  // Draw the string, aligning it based on the sprite's text datum.
  if (sprite.getTextDatum() == ML_DATUM)
  {
    sprite.drawString(text, 0, sprite.height() / 2);
  }
  else if (sprite.getTextDatum() == MR_DATUM)
  {
    sprite.drawString(text, sprite.width(), sprite.height() / 2);
  }
  else
  { // Assumes MC_DATUM or other centered datums
    sprite.drawString(text, sprite.width() / 2, sprite.height() / 2);
  }

  // Push the final sprite to the specified coordinates on the screen.
  sprite.pushSprite(x, y);
}

void Display::drawDayOfWeek(const char *dayStr)
{
  // Only redraw if the day has changed.
  if (String(dayStr) != lastDayOfWeek)
  {
    updateSprite(sprDayOfWeek, dayStr, MARGIN, dateY);
    lastDayOfWeek = dayStr; // Cache the new day.
  }
}

void Display::drawDate(const char *dateStr)
{
  // Only redraw if the date has changed.
  if (String(dateStr) != lastDate)
  {
    updateSprite(sprDate, dateStr, tft.width() / 2, dateY);
    lastDate = dateStr; // Cache the new date.
  }
}

void Display::drawTemperature(float temp, bool isCelsius)
{
  // Format the temperature string with the correct unit.
  char buf[16];
  char unit = isCelsius ? 'C' : 'F';
  snprintf(buf, sizeof(buf), "%.0f%c", temp, unit);
  // Update the sprite. Note: No caching is used here; it redraws every time.
  updateSprite(sprTemp, buf, MARGIN, sensorY);
}

void Display::drawHumidity(float humidity)
{
  // Format the humidity string with a percent sign.
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f%%", humidity);
  // Update the sprite. Note: No caching is used here.
  updateSprite(sprHumidity, buf, tft.width() / 2, sensorY);
}

void Display::drawStatusMessage(const char *message)
{
  // This function takes over the entire screen to display a message.
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

  // Check if auto-brightness is enabled.
  if (config.isAutoBrightness())
  {
    uint8_t hour = TimeManager::getInstance().getHour();
    // Simple time-based schedule: bright during the day, dim at night.
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
    // Use the manually set brightness value.
    dutyCycle = config.getBrightness();
  }
  // Apply the calculated brightness value to the backlight LED.
  ledcWrite(BACKLIGHT_CHANNEL, dutyCycle);
}