/**
 * @file DisplayManager.cpp
 * @brief Implements the DisplayManager class for high-level UI control.
 *
 * This file contains the implementation for managing different display pages,
 * handling page transitions, and orchestrating the rendering of UI elements
 * like the alarm icon and error screens.
 */

#include "DisplayManager.h"
#include "constants.h"
#include "Utils.h"
#include "ConfigManager.h"

/**
 * @brief Initializes the DisplayManager.
 * @param tft_instance A reference to the main TFT_eSPI object.
 */
void DisplayManager::begin(TFT_eSPI &tft_instance)
{
  this->_tft = &tft_instance;
}

#include <utility> // For std::move

/**
 * @brief Adds a new page to the manager.
 *
 * The manager takes ownership of the page via a `std::unique_ptr`.
 *
 * @param page A `std::unique_ptr` to the Page object to be added.
 */
void DisplayManager::addPage(std::unique_ptr<Page> page)
{
  _pages.push_back(std::move(page));
}

/**
 * @brief Sets the currently active page.
 *
 * Handles the page transition by calling `onExit()` on the old page and
 * `onEnter()` on the new page.
 *
 * @param index The index of the page to activate.
 * @param forceRedraw If true, forces the page to exit and re-enter even if it's the same page.
 */
void DisplayManager::setPage(int index, bool forceRedraw)
{
  if (index < 0 || index >= _pages.size())
  {
    return; // Index out of bounds
  }

  // If the page isn't changing and we're not forcing a redraw, do nothing.
  if (_currentPageIndex == index && !forceRedraw)
  {
    return;
  }

  if (_currentPage)
  {
    _currentPage->onExit();
  }

  _currentPageIndex = index;
  _currentPage = _pages[_currentPageIndex].get();
  _currentPage->onEnter(*_tft);

  // After a page change, the screen is cleared, so the icon is no longer visible.
  // We need to reset its state to force a redraw on the next loop.
  _alarmIconVisible = false;
}

/**
 * @brief Updates and renders the currently active page.
 *
 * This should be called in the main application loop. It handles refresh
 * requests and then calls the `update()` and `render()` methods of the
 * current page.
 */
void DisplayManager::update()
{
  if (_fullRefresh)
  {
    if (_currentPage)
    {
      _currentPage->refresh(*_tft, true);
    }
    _fullRefresh = false;
    _partialRefresh = false; // A full refresh implies a partial one
    // After a full refresh, we must force the alarm icon to be redrawn.
    _alarmIconVisible = false;
  }
  else if (_partialRefresh)
  {
    if (_currentPage)
    {
      _currentPage->refresh(*_tft, false);
    }
    _partialRefresh = false;
  }

  if (_currentPage)
  {
    _currentPage->update();
    _currentPage->render(*_tft);
  }
}

/**
 * @brief Requests a partial refresh of the display.
 *
 * A partial refresh typically updates dynamic content without redrawing the
 * background, which is faster and reduces flicker.
 */
void DisplayManager::requestPartialRefresh()
{
  _partialRefresh = true;
}

/**
 * @brief Requests a full refresh of the display.
 *
 * A full refresh redraws the entire screen, including the background.
 * This is used when major visual changes occur, like a color scheme update.
 */
void DisplayManager::requestFullRefresh()
{
  _fullRefresh = true;
}

/**
 * @brief Legacy method to refresh the screen. Now maps to a partial refresh.
 */
void DisplayManager::refresh()
{
  // This method is now deprecated, but we can keep it for compatibility.
  // For now, we'll just trigger a partial refresh.
  requestPartialRefresh();
}

/**
 * @brief Gets the number of pages managed by the DisplayManager.
 * @return The number of pages.
 */
int DisplayManager::getPagesSize()
{
  return _pages.size();
}

/**
 * @brief Draws or erases the alarm indicator icon.
 *
 * This function handles drawing the small bell icon in the corner of the
 * display. It uses different colors to indicate whether an alarm is set or
 * currently snoozed.
 *
 * @param enabled True to show the icon, false to hide it.
 * @param snoozing True to use the snooze color, false for the standard alarm color.
 */
void DisplayManager::drawAlarmIcon(bool enabled, bool snoozing)
{
  if (_alarmIconVisible == enabled && _isSnoozing == snoozing)
  {
    return; // No change needed
  }
  _alarmIconVisible = enabled;
  _isSnoozing = snoozing;

  if (enabled)
  {
    String colorHex = snoozing ? ConfigManager::getInstance().getSnoozeIconColor() : ConfigManager::getInstance().getAlarmIconColor();
    uint16_t color = hexToRGB565(colorHex.c_str());
    // Bell body using a rounded rectangle
    _tft->fillRoundRect(ALARM_ICON_X + 2, ALARM_ICON_Y, 12, 11, 4, color);
    // Bell lip/flare
    _tft->fillRect(ALARM_ICON_X, ALARM_ICON_Y + 10, 16, 3, color);
    // Clapper (small circle inside)
    _tft->fillCircle(ALARM_ICON_X + 8, ALARM_ICON_Y + 12, 2, hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  }
  else
  {
    // Erase the icon by drawing a black rectangle over its bounding box
    _tft->fillRect(ALARM_ICON_X, ALARM_ICON_Y, ALARM_ICON_WIDTH, ALARM_ICON_HEIGHT, hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  }
}

/**
 * @brief Displays a critical error message and halts.
 *
 * This function is used for fatal errors. It clears the screen and displays
 * the error message in a prominent color. This is a blocking call.
 *
 * @param message The error message to display.
 */
void DisplayManager::showErrorScreen(const char *message)
{
  _tft->fillScreen(hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getErrorTextColor().c_str()));
  _tft->drawString(message, _tft->width() / 2, _tft->height() / 2, 4); // Use font 4 for a clear message

  // Reset text datum and color for any potential subsequent drawing
  _tft->setTextDatum(TL_DATUM);
  _tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getTimeColor().c_str()));
}