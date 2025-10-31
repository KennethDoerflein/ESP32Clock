/**
 * @file Display.cpp
 * @brief Implements the Display class for low-level screen and backlight control.
 *
 * This file contains the implementation for initializing the TFT display,
 * managing the backlight brightness using PWM, and drawing simple status
 * messages to the screen.
 */

#include "Display.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "fonts/CenturyGothic28.h"
#include <Arduino.h>

// LEDC (LED Control) constants for managing the backlight PWM.
#define TFT_BL 6               // Manually define the backlight pin here
#define BACKLIGHT_CHANNEL 0    // LEDC channel 0
#define BACKLIGHT_FREQ 5000    // 5 kHz frequency
#define BACKLIGHT_RESOLUTION 8 // 8-bit resolution (0-255)

/**
 * @brief Initializes the display and backlight.
 *
 * This function sets up the LEDC peripheral for PWM control of the backlight,
 * attaches the backlight pin, and initializes the TFT display driver.
 * It also sets a default full brightness and clears the screen.
 */
void Display::begin()
{
  // Configure the LEDC peripheral for PWM control of the backlight.
  ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
  ledcAttachPin(TFT_BL, BACKLIGHT_CHANNEL);

  // Set a default brightness so the screen is on during boot.
  ledcWrite(BACKLIGHT_CHANNEL, 255);

  // Initialize the TFT display driver.
  tft.init();
  updateRotation();
  updateInversion();
  tft.fillScreen(TFT_BLACK);
}

/**
 * @brief Updates the screen's rotation based on the current configuration.
 *
 * Reads the `screenFlipped` setting from the ConfigManager and sets the
 * TFT rotation accordingly.
 */
void Display::updateRotation()
{
  tft.setRotation(ConfigManager::getInstance().isScreenFlipped() ? 1 : 3);
}

/**
 * @brief Updates the screen's color inversion based on the current configuration.
 *
 * Reads the `invertColors` setting from the ConfigManager and sets the
 * TFT inversion accordingly.
 */
void Display::updateInversion()
{
  tft.invertDisplay(ConfigManager::getInstance().isInvertColors());
}

/**
 * @brief Displays a single-line status message centered on the screen.
 * @param message The message to display.
 */
void Display::drawStatusMessage(const char *message)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(CenturyGothic28);
  tft.drawString(message, tft.width() / 2, tft.height() / 2);
  tft.unloadFont(); // Unload font to free up memory
}

/**
 * @brief Displays a two-line status message centered on the screen.
 * @param line1 The first line of the message.
 * @param line2 The second line of the message.
 */
void Display::drawMultiLineStatusMessage(const char *line1, const char *line2)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.loadFont(CenturyGothic28);
  tft.drawString(line1, tft.width() / 2, tft.height() / 2 - 15);
  tft.drawString(line2, tft.width() / 2, tft.height() / 2 + 15);
  tft.unloadFont(); // Unload font to free up memory
}

/**
 * @brief Enables or disables the backlight flashing effect.
 *
 * This is used to provide a visual cue when an alarm is ringing.
 *
 * @param enabled True to enable flashing, false to disable.
 */
void Display::setBacklightFlashing(bool enabled)
{
  _isFlashing = enabled;
  if (!_isFlashing)
  {
    // If flashing is disabled, immediately restore the correct brightness
    updateBrightness();
  }
}

/**
 * @brief Updates the backlight brightness based on current settings.
 *
 * This function implements the logic for both manual and automatic brightness
 * control. It reads the settings from the ConfigManager, determines the
 * appropriate brightness level, and applies it to the backlight PWM channel.
 * It also handles the flashing effect when an alarm is active.
 */
void Display::updateBrightness()
{
  if (_isFlashing)
  {
    unsigned long currentTime = millis();
    if (currentTime - _lastFlashTime > 500) // 500ms interval
    {
      _lastFlashTime = currentTime;
      // Toggle between high and low brightness
      if (actualBrightness > 10)
      {
        actualBrightness = 10;
      }
      else
      {
        actualBrightness = 255;
      }
      ledcWrite(BACKLIGHT_CHANNEL, actualBrightness);
    }
    return; // Skip normal brightness logic
  }

  auto &config = ConfigManager::getInstance();
  int dutyCycle;

  // Check if auto-brightness is enabled.
  if (config.isAutoBrightness())
  {
    uint8_t hour = TimeManager::getInstance().getHour();
    uint8_t startHour = config.getAutoBrightnessStartHour();
    uint8_t endHour = config.getAutoBrightnessEndHour();

    // Determine if the current time is within the "day" period.
    bool isDayTime;
    if (startHour < endHour)
    {
      // Standard day period (e.g., 7 AM to 9 PM)
      isDayTime = (hour >= startHour && hour < endHour);
    }
    else
    {
      // Overnight day period (e.g., 9 PM to 7 AM)
      isDayTime = (hour >= startHour || hour < endHour);
    }

    if (isDayTime)
    {
      dutyCycle = config.getDayBrightness();
    }
    else
    {
      dutyCycle = config.getNightBrightness();
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