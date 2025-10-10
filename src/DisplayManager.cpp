#include "DisplayManager.h"

void DisplayManager::begin(TFT_eSPI &tft_instance)
{
  this->tft = &tft_instance;
}

void DisplayManager::addPage(Page *page)
{
  pages.push_back(page);
}

void DisplayManager::setPage(int index)
{
  if (index < 0 || index >= pages.size())
  {
    return; // Index out of bounds
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