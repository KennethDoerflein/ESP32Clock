#include "DisplayManager.h"

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
}

void DisplayManager::update()
{
  if (currentPage)
  {
    currentPage->update();
    currentPage->render(*tft);
  }
}

int DisplayManager::getPagesSize()
{
  return pages.size();
}

void DisplayManager::drawAlarmIcon(bool enabled)
{
  if (_alarmIconVisible == enabled)
  {
    return; // No change needed
  }
  _alarmIconVisible = enabled;

  if (enabled)
  {
    // Draw a simple bell icon (a circle with a dot)
    tft->fillCircle(470, 10, 5, TFT_YELLOW);
    tft->fillRect(465, 15, 10, 2, TFT_YELLOW);
  }
  else
  {
    // Erase the icon by drawing a black rectangle over it
    tft->fillRect(460, 5, 15, 15, TFT_BLACK);
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