#include "DisplayManager.h"
#include "utils.h"
#include "ConfigManager.h"

void DisplayManager::begin(TFT_eSPI &tft_instance)
{
  this->tft = &tft_instance;
}

#include <utility> // For std::move

void DisplayManager::addPage(std::unique_ptr<Page> page)
{
  pages.push_back(std::move(page));
}

void DisplayManager::setPage(int index, bool forceRedraw)
{
  if (index < 0 || index >= pages.size())
  {
    return; // Index out of bounds
  }

  // If the page isn't changing and we're not forcing a redraw, do nothing.
  if (currentPageIndex == index && !forceRedraw)
  {
    return;
  }

  if (currentPage)
  {
    currentPage->onExit();
  }

  currentPageIndex = index;
  currentPage = pages[currentPageIndex].get();
  currentPage->onEnter(*tft);

  // After a page change, the screen is cleared, so the icon is no longer visible.
  // We need to reset its state to force a redraw on the next loop.
  _alarmIconVisible = false;
}

void DisplayManager::update()
{
  if (_fullRefresh)
  {
    if (currentPage)
    {
      currentPage->refresh(*tft, true);
    }
    _fullRefresh = false;
    _partialRefresh = false; // A full refresh implies a partial one
  }
  else if (_partialRefresh)
  {
    if (currentPage)
    {
      currentPage->refresh(*tft, false);
    }
    _partialRefresh = false;
  }

  if (currentPage)
  {
    currentPage->update();
    currentPage->render(*tft);
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
  return pages.size();
}

void DisplayManager::drawAlarmIcon(bool enabled, bool snoozing)
{
  if (_alarmIconVisible == enabled && _isSnoozing == snoozing)
  {
    return; // No change needed
  }
  _alarmIconVisible = enabled;
  _isSnoozing = snoozing;

  // Define the bounding box for the icon for easy clearing
  const int icon_x = 462;
  const int icon_y = 5;
  const int icon_w = 18;
  const int icon_h = 18;

  if (enabled)
  {
    uint16_t color = snoozing ? TFT_BLUE : TFT_YELLOW;
    // Bell body using a rounded rectangle
    tft->fillRoundRect(icon_x + 2, icon_y, 12, 11, 4, color);
    // Bell lip/flare
    tft->fillRect(icon_x, icon_y + 10, 16, 3, color);
    // Clapper (small circle inside)
    tft->fillCircle(icon_x + 8, icon_y + 12, 2, TFT_BLACK);
  }
  else
  {
    // Erase the icon by drawing a black rectangle over its bounding box
    tft->fillRect(icon_x, icon_y, icon_w, icon_h, TFT_BLACK);
  }
}

void DisplayManager::showAlarmScreen()
{
  // This is a simple overlay, it doesn't create a new page.
  // It draws a large "RINGING!" message in the center of the screen.
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_RED);
  tft->drawString("RINGING!", tft->width() / 2, tft->height() / 2, 7);

  // Reset text datum and color for other components
  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(TFT_WHITE);
}

void DisplayManager::showErrorScreen(const char *message)
{
  tft->fillScreen(TFT_BLACK);
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(TFT_RED);
  tft->drawString(message, tft->width() / 2, tft->height() / 2, 4); // Use font 4 for a clear message

  // Reset text datum and color for any potential subsequent drawing
  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(TFT_WHITE);
}