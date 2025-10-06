// display.h

#pragma once

#include <TFT_eSPI.h>

/**
 * @class Display
 * @brief Manages all operations related to the TFT display.
 *
 * This singleton class handles the initialization of the TFT screen,
 * drawing all UI elements (clock, date, sensor data), managing screen layout,
 * and controlling the backlight brightness. It uses sprites for efficient,
 * flicker-free updates.
 */
class Display
{
public:
  /**
   * @brief Gets the singleton instance of the Display class.
   * @return A reference to the singleton Display instance.
   */
  static Display &getInstance()
  {
    static Display instance;
    return instance;
  }

  /**
   * @brief Initializes the display and backlight control.
   * This sets up the TFT driver, sprites, and PWM for the backlight.
   */
  void begin();

  /**
   * @brief Draws the main clock time on the screen.
   * @param timeStr The time string to display (e.g., "10:30").
   * @param todStr The time-of-day string ("AM"/"PM") or empty for 24h format.
   */
  void drawClock(const char *timeStr, const char *todStr);

  /**
   * @brief Draws the date on the screen.
   * @param dateStr The date string to display (e.g., "OCT 26").
   */
  void drawDate(const char *dateStr);

  /**
   * @brief Draws the day of the week on the screen.
   * @param dayStr The day of the week string (e.g., "THU").
   */
  void drawDayOfWeek(const char *dayStr);

  /**
   * @brief Draws the temperature reading on the screen.
   * @param temp The temperature value.
   * @param isCelsius True to display the unit as 'C', false for 'F'.
   */
  void drawTemperature(float temp, bool isCelsius);

  /**
   * @brief Draws the humidity reading on the screen.
   * @param humidity The humidity value.
   */
  void drawHumidity(float humidity);

  /**
   * @brief Displays a status message in the center of the screen.
   * This clears the screen and shows a single message, useful for startup or errors.
   * @param message The message to display.
   */
  void drawStatusMessage(const char *message);

  /**
   * @brief Redraws the basic screen layout.
   * Currently clears the screen to black.
   */
  void drawLayout();

  /**
   * @brief Updates the screen's backlight brightness.
   * Adjusts brightness based on manual settings or auto-brightness schedule.
   */
  void updateBrightness();

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  Display() {}

  TFT_eSPI tft;                   ///< The main TFT_eSPI driver instance.
  TFT_eSprite sprClock{&tft};     ///< Sprite for the main clock display.
  TFT_eSprite sprDayOfWeek{&tft}; ///< Sprite for the day of the week.
  TFT_eSprite sprDate{&tft};      ///< Sprite for the date.
  TFT_eSprite sprTemp{&tft};      ///< Sprite for the temperature.
  TFT_eSprite sprHumidity{&tft};  ///< Sprite for the humidity.
  TFT_eSprite sprTOD{&tft};       ///< Sprite for the AM/PM indicator.

  /**
   * @brief Creates and configures all sprites for the UI elements.
   */
  void setupSprites();

  /**
   * @brief Calculates the initial Y positions for the UI elements.
   */
  void setupLayout();

  /**
   * @brief A helper function to update and push a sprite to the screen.
   * @param sprite The sprite to update.
   * @param text The new text to draw on the sprite.
   * @param x The X coordinate to push the sprite to.
   * @param y The Y coordinate to push the sprite to.
   */
  void updateSprite(TFT_eSprite &sprite, const char *text, int x, int y);

  // --- Caching variables to prevent unnecessary redraws ---
  String lastTime = "";
  String lastDate = "";
  String lastDayOfWeek = "";
  String lastTOD = "--";

  // --- Layout position variables ---
  int clockY;
  int dateY;
  int sensorY;
};