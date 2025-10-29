#include "DisplayManager.h"
#include "constants.h"
#include "utils.h"
#include "ConfigManager.h"

void DisplayManager::begin(TFT_eSPI &tft_instance)
{
  this->_tft = &tft_instance;
}

#include <utility> // For std::move

void DisplayManager::addPage(std::unique_ptr<Page> page)
{
  _pages.push_back(std::move(page));
}

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

void DisplayManager::requestPartialRefresh()
{
  _partialRefresh = true;
}

void DisplayManager::requestFullRefresh()
{
  _fullRefresh = true;
}

void DisplayManager::refresh()
{
  // This method is now deprecated, but we can keep it for compatibility.
  // For now, we'll just trigger a partial refresh.
  requestPartialRefresh();
}

int DisplayManager::getPagesSize()
{
  return _pages.size();
}

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