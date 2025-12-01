/**
 * @file DisplayManager.cpp
 * @brief Implements the DisplayManager class for high-level UI control.
 *
 * This file contains the implementation for managing different display pages,
 * handling page transitions, and orchestrating the rendering of UI elements
 * like the alarm icon and error screens.
 */

#include "DisplayManager.h"
#include "Constants.h"
#include "Utils.h"
#include "ConfigManager.h"
#include "Display.h"
#include "AlarmManager.h"
#include "TimeManager.h"
#include "fonts/CenturyGothicBold48.h"
#include "SerialLog.h"

/**
 * @brief Private constructor to enforce the singleton pattern.
 */
DisplayManager::DisplayManager() : _tft(nullptr), _currentPage(nullptr), _alarmSprite(nullptr) {}

/**
 * @brief Initializes the DisplayManager.
 * @param tft_instance A reference to the main TFT_eSPI object.
 */
void DisplayManager::begin(TFT_eSPI &tft_instance)
{
  this->_tft = &tft_instance;
  initAlarmSprite();
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

  Display::getInstance().lock();
  if (_currentPage)
  {
    _currentPage->onExit();
  }

  _currentPageIndex = index;
  _currentPage = _pages[_currentPageIndex].get();
  _currentPage->onEnter(*_tft);
  Display::getInstance().unlock();

  // After a page change, the screen is cleared, so the icon is no longer visible.
  // We need to reset its state to force a redraw on the next loop.
  _alarmIconVisible = false;

  // Also force alarm overlay redraw if active
  if (AlarmManager::getInstance().isRinging() || ConfigManager::getInstance().isAnyAlarmSnoozed())
  {
    _wasAlarmActive = false; // Will trigger redraw in next update
  }
}

/**
 * @brief Cycles to the next page in the configured sequence.
 */
void DisplayManager::cyclePage()
{
  auto &config = ConfigManager::getInstance();
  std::vector<int> enabled = config.getEnabledPages();

  if (enabled.empty())
  {
    // Fallback if nothing enabled
    setPage(0);
    return;
  }

  int currentIndex = _currentPageIndex;
  int nextIndex = -1;

  // Find current index in the enabled list
  for (size_t i = 0; i < enabled.size(); ++i)
  {
    if (enabled[i] == currentIndex)
    {
      // Found it, go to next
      nextIndex = enabled[(i + 1) % enabled.size()];
      break;
    }
  }

  if (nextIndex == -1)
  {
    // Current page not in list, go to first enabled
    nextIndex = enabled[0];
  }

  SerialLog::getInstance().printf("Cycling to page index: %d\n", nextIndex);
  setPage(nextIndex);
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
  Display::getInstance().lock();
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
    _wasAlarmActive = false; // Force alarm overlay redraw
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

  // Render Alarm Overlay on top of everything
  renderAlarmOverlay();

  Display::getInstance().unlock();
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

  Display::getInstance().lock();
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
  Display::getInstance().unlock();
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
  Display::getInstance().lock();
  _tft->fillScreen(hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getErrorTextColor().c_str()));
  _tft->drawString(message, _tft->width() / 2, _tft->height() / 2, 4); // Use font 4 for a clear message

  // Reset text datum and color for any potential subsequent drawing
  _tft->setTextDatum(TL_DATUM);
  _tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getTimeColor().c_str()));
  Display::getInstance().unlock();
}

void DisplayManager::initAlarmSprite()
{
  if (_alarmSprite == nullptr)
  {
    _alarmSprite = new TFT_eSprite(_tft);
    _tft->loadFont(CenturyGothicBold48);
    int textWidth = _tft->textWidth("ALARM");
    _alarmSprite->createSprite(textWidth + ALARM_SPRITE_WIDTH_PADDING, ALARM_SPRITE_HEIGHT);
    _tft->unloadFont();
  }
}

void DisplayManager::renderAlarmOverlay()
{
  auto &alarmManager = AlarmManager::getInstance();
  auto &config = ConfigManager::getInstance();
  bool isAlarmActive = alarmManager.isRinging() || config.isAnyAlarmSnoozed();

  if (!isAlarmActive)
  {
    if (_wasAlarmActive)
    {
      // Clear the overlay
      clearAlarmOverlay();
      _wasAlarmActive = false;
      requestPartialRefresh();
    }
    return;
  }

  _wasAlarmActive = true;

  uint16_t alarmColor = hexToRGB565(config.getAlarmTextColor().c_str());
  uint16_t bgColor = hexToRGB565(config.getBackgroundColor().c_str());

  // Clear the sprite with the background color
  _alarmSprite->fillSprite(bgColor);

  _alarmSprite->loadFont(CenturyGothicBold48);
  _alarmSprite->setTextDatum(MC_DATUM);

  bool showButton = false;
  String text = "";

  if (alarmManager.isRinging())
  {
    showButton = true;
    text = "ALARM";
  }
  else
  {
    // Check for snoozed alarms
    for (int i = 0; i < config.getNumAlarms(); ++i)
    {
      Alarm alarm = config.getAlarmByIndex(i);
      if (alarm.isSnoozed())
      {
        time_t snoozeUntil = alarm.getSnoozeUntil();
        time_t now = TimeManager::getInstance().getRTCTime().unixtime();
        long remaining = snoozeUntil - now;
        if (remaining < 0)
        {
          remaining = 0;
        }

        char buf[10];
        snprintf(buf, sizeof(buf), "%ld:%02ld", remaining / 60, remaining % 60);
        text = String(buf);
        showButton = true;
        break;
      }
    }
  }

  if (showButton)
  {
    // Draw the rounded button body
    _alarmSprite->fillRoundRect(0, 0, _alarmSprite->width(), _alarmSprite->height(), 10, alarmColor);

    // Set text color to background color (inverted)
    _alarmSprite->setTextColor(bgColor);
    _alarmSprite->drawString(text.c_str(), _alarmSprite->width() / 2, _alarmSprite->height() / 2);

    // Draw the progress bar if the button is being held
    if (_dismissProgress > 0.0f)
    {
      int margin = 5;
      int availableWidth = _alarmSprite->width() - (2 * margin);
      int barWidth = availableWidth * _dismissProgress;
      _alarmSprite->fillRoundRect(margin, _alarmSprite->height() - ALARM_PROGRESS_BAR_HEIGHT - margin, barWidth, ALARM_PROGRESS_BAR_HEIGHT, 3, TFT_WHITE);
    }
  }
  else
  {
    _dismissProgress = 0.0f;
  }

  int screenWidth = _tft->width();
  int screenHeight = _tft->height();
  int x = (screenWidth - _alarmSprite->width()) / 2;
  int y = (screenHeight - _alarmSprite->height()) / 2;

  _alarmSprite->pushSprite(x, y);
}

void DisplayManager::setDismissProgress(float progress)
{
  _dismissProgress = progress;
}

void DisplayManager::clearAlarmOverlay()
{
  if (_alarmSprite)
  {
    // Calculate position (must match renderAlarmOverlay)
    int screenWidth = _tft->width();
    int screenHeight = _tft->height();
    int x = (screenWidth - _alarmSprite->width()) / 2;
    int y = (screenHeight - _alarmSprite->height()) / 2;

    uint16_t bgColor = hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str());
    _tft->fillRect(x, y, _alarmSprite->width(), _alarmSprite->height(), bgColor);
  }
}

void DisplayManager::showAlarmScreen()
{
  // Deprecated/Unused with new overlay logic, but kept for interface compatibility
}

void DisplayManager::drawDismissProgressBar(float progress)
{
  // Delegated to renderAlarmOverlay via state
  setDismissProgress(progress);
  renderAlarmOverlay();
}
