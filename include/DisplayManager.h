#pragma once

#include "Page.h"
#include <vector>
#include <memory> // For std::unique_ptr
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
   * @param page A unique_ptr to the Page object to add.
   */
  void addPage(std::unique_ptr<Page> page);

  /**
   * @brief Sets the currently active page by its index.
   * @param index The index of the page to activate.
   * @param forceRedraw If true, forces the page to exit and re-enter.
   */
  void setPage(int index, bool forceRedraw = false);

  /**
   * @brief Gets the number of pages managed.
   * @return The total number of pages.
   */
  int getPagesSize();

  /**
   * @brief Gets the index of the currently active page.
   * @return The index of the current page.
   */
  int getCurrentPageIndex() const { return currentPageIndex; }

  /**
   * @brief Updates the currently active page.
   * This should be called in the main application loop.
   */
  void update();

  /**
   * @brief Forces the current page to refresh its content.
   */
  void refresh();

  /**
   * @brief Requests a partial refresh of the display (foreground only).
   */
  void requestPartialRefresh();

  /**
   * @brief Requests a full refresh of the display (background + foreground).
   */
  void requestFullRefresh();

  /**
   * @brief Draws or erases the alarm indicator icon on the display.
   * @param enabled True to draw the icon, false to erase it.
   * @param snoozing If true, draw the icon in a snoozing state (e.g., different color).
   */
  void drawAlarmIcon(bool enabled, bool snoozing);

  /**
   * @brief Switches the display to the "ringing" screen.
   */
  void showAlarmScreen();

private:
  DisplayManager() : tft(nullptr), currentPage(nullptr) {}

  TFT_eSPI *tft;                            ///< Pointer to the main TFT object.
  std::vector<std::unique_ptr<Page>> pages; ///< A list of all available pages.
  Page *currentPage;                        ///< Pointer to the currently active page.
  int currentPageIndex = -1;                ///< Index of the current page.
  bool _alarmIconVisible = false;           ///< Tracks the current state of the alarm icon.
  bool _isSnoozing = false;                 ///< Tracks the snooze state of the icon.
  bool _partialRefresh = false;
  bool _fullRefresh = false;
};