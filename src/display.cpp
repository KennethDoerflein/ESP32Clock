#include "display.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "fonts/DSEG14ModernBold24.h"
#include <Arduino.h>

// LEDC (LED Control) constants for managing the backlight PWM.
#define TFT_BL 6               // Manually define the backlight pin here
#define BACKLIGHT_CHANNEL 0    // LEDC channel 0
#define BACKLIGHT_FREQ 5000    // 5 kHz frequency
#define BACKLIGHT_RESOLUTION 8 // 8-bit resolution (0-255)

void Display::begin()
{
  // Configure the LEDC peripheral for PWM control of the backlight.
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
  ledcAttachPin(TFT_BL, BACKLIGHT_CHANNEL);

  // Set a default brightness so the screen is on during boot.
  ledcWrite(BACKLIGHT_CHANNEL, 255);

  // Initialize the TFT display driver.
  tft.init();
  tft.setRotation(3); // Set screen rotation.
  tft.fillScreen(TFT_BLACK);
}

void Display::drawStatusMessage(const char *message)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(DSEG14ModernBold24);
  tft.drawString(message, tft.width() / 2, tft.height() / 2);
  tft.unloadFont(); // Unload font to free up memory
}

void Display::drawMultiLineStatusMessage(const char *line1, const char *line2)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(DSEG14ModernBold24);
  tft.drawString(line1, tft.width() / 2, tft.height() / 2 - 15);
  tft.drawString(line2, tft.width() / 2, tft.height() / 2 + 15);
  tft.unloadFont(); // Unload font to free up memory
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
      dutyCycle = 10; // Dim brightness (about 4%)
    }
  }
  else
  {
    if (config.getBrightness() > 255)
    {
      dutyCycle = 255; // Clamp to max value
    }
    else if (config.getBrightness() < 10)
    {
      dutyCycle = 10; // Clamp to min value
    }
    else
    {
      // Use the manually set brightness value.
      dutyCycle = config.getBrightness();
    }
  }

  actualBrightness = dutyCycle; // Store the calculated brightness

  // Apply the calculated brightness value to the backlight LED.
  ledcWrite(BACKLIGHT_CHANNEL, dutyCycle);
}