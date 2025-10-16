#pragma once

#include <TFT_eSPI.h>

/**
 * @class Display
 * @brief Manages the low-level TFT display and backlight.
 *
 * This singleton class handles the initialization of the TFT screen driver
 * and controls the backlight brightness. It no longer manages UI content,
 * which is now handled by the DisplayManager.
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
   */
  void begin();

  /**
   * @brief Updates the screen's backlight brightness.
   */
  void updateBrightness();

  /**
   * @brief Displays a status message in the center of the screen.
   * @param message The message to display.
   */
  void drawStatusMessage(const char *message);

  /**
   * @brief Gets the last calculated brightness value.
   * @return The current brightness duty cycle (0-255).
   */
  int getActualBrightness() const { return actualBrightness; }

  /**
   * @brief Provides access to the underlying TFT_eSPI object.
   * @return A reference to the TFT_eSPI instance.
   */
  TFT_eSPI &getTft() { return tft; }

private:
  Display() : actualBrightness(255) {} // Initialize to a default value

  TFT_eSPI tft;         ///< The main TFT_eSPI driver instance.
  int actualBrightness; ///< Stores the current brightness duty cycle.
};