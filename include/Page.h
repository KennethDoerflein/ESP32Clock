#pragma once

#include <TFT_eSPI.h>

/**
 * @class Page
 * @brief An abstract base class for a single display page.
 *
 * This class defines the interface for different UI pages that can be managed
 * by the DisplayManager. Each derived class will implement the specific logic
 * for rendering and updating its content.
 */
class Page
{
public:
  virtual ~Page() {}

  /**
   * @brief Called once when the page becomes active.
   * Use this to set up sprites, clear the screen, or draw static elements.
   * @param tft A reference to the main TFT_eSPI object.
   */
  virtual void onEnter(TFT_eSPI &tft) = 0;

  /**
   * @brief Called once when the page is switched away from.
   * Use this for any necessary cleanup.
   */
  virtual void onExit() = 0;

  /**
   * @brief Called repeatedly in the main loop to update page content.
   * This is where dynamic data is processed and redraws are triggered.
   */
  virtual void update() = 0;

  /**
   * @brief Called repeatedly in the main loop to render the page on the screen.
   * This handles the actual drawing commands.
   * @param tft A reference to the main TFT_eSPI object.
   */
  virtual void render(TFT_eSPI &tft) = 0;
};