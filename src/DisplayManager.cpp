#include "DisplayManager.h"

void DisplayManager::begin(TFT_eSPI &tft_instance)
{
  this->tft = &tft_instance;
}

void DisplayManager::addPage(Page *page)
{
  pages.push_back(page);
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
  currentPage = pages[currentPageIndex];
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