#pragma once

#include "Page.h"
#include <vector>
#include <TFT_eSPI.h>

/**
 * @class DisplayManager
 * @brief Manages the active display page and orchestrates rendering.
 *
 * This singleton class holds a collection of Page objects, tracks the current
 * page, and calls its update and render methods. It acts as the high-level
 * controller for the entire display logic.
 */
class DisplayManager
{
public:
  /**
   * @brief Gets the singleton instance of the DisplayManager.
   * @return A reference to the DisplayManager instance.
   */
  static DisplayManager &getInstance()
  {
    static DisplayManager instance;
    return instance;
  }

  /**
   * @brief Initializes the DisplayManager.
   * @param tft A reference to the main TFT_eSPI object.
   */
  void begin(TFT_eSPI &tft);

  /**
   * @brief Adds a page to the manager's collection.
   * @param page A pointer to the Page object to add.
   */
  void addPage(Page *page);

  /**
   * @brief Sets the currently active page by its index.
   * @param index The index of the page to activate.
   */
  void setPage(int index);

  /**
   * @brief Gets the number of pages managed.
   * @return The total number of pages.
   */
  int getPagesSize();

  /**
   * @brief Updates the currently active page.
   * This should be called in the main application loop.
   */
  void update();

private:
  DisplayManager() : tft(nullptr), currentPage(nullptr) {}

  TFT_eSPI *tft;             ///< Pointer to the main TFT object.
  std::vector<Page *> pages; ///< A list of all available pages.
  Page *currentPage;         ///< Pointer to the currently active page.
  int currentPageIndex = -1; ///< Index of the current page.
};