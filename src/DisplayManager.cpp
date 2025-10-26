#include "DisplayManager.h"
#include "utils.h"
#include "ConfigManager.h"

// A vertical offset to visually center the "ALARM" text.
constexpr int ALARM_TEXT_VISUAL_CENTER_VOFFSET = -15;

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
    String colorHex = snoozing ? ConfigManager::getInstance().getSnoozeIconColor() : ConfigManager::getInstance().getAlarmIconColor();
    uint16_t color = hexToRGB565(colorHex.c_str());
    // Bell body using a rounded rectangle
    tft->fillRoundRect(icon_x + 2, icon_y, 12, 11, 4, color);
    // Bell lip/flare
    tft->fillRect(icon_x, icon_y + 10, 16, 3, color);
    // Clapper (small circle inside)
    tft->fillCircle(icon_x + 8, icon_y + 12, 2, hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  }
  else
  {
    // Erase the icon by drawing a black rectangle over its bounding box
    tft->fillRect(icon_x, icon_y, icon_w, icon_h, hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  }
}

void DisplayManager::showAlarmScreen()
{
  // This is a simple overlay, it doesn't create a new page.
  // It draws a large "RINGING!" message in the center of the screen.
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getAlarmTextColor().c_str()));
  tft->drawString("ALARM", tft->width() / 2, (tft->height() / 2) + ALARM_TEXT_VISUAL_CENTER_VOFFSET, 7);

  // Reset text datum and color for other components
  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getTimeColor().c_str()));
}

void DisplayManager::drawDismissProgressBar(float progress)
{
  // Define the bar's dimensions and position it relative to the "ALARM" text.
  const int barHeight = 10;
  // Font 7 is used for "ALARM", its height is 48 pixels.
  // We'll place the bar a few pixels below the text's vertical center.
  const int barY = (tft->height() / 2) + 30 + ALARM_TEXT_VISUAL_CENTER_VOFFSET; // 30 pixels below center

  // The width of the bar will match the approximate width of the text.
  const int barWidth = tft->textWidth("ALARM", 7);
  const int barX = (tft->width() / 2) - (barWidth / 2);

  // Calculate the width of the progress portion
  int progressWidth = static_cast<int>(barWidth * progress);

  // Draw the progress bar
  uint16_t color = hexToRGB565(ConfigManager::getInstance().getAlarmTextColor().c_str());
  tft->fillRect(barX, barY, progressWidth, barHeight, color);

  // Clear the remainder of the bar to handle cases where progress decreases (though not expected here)
  uint16_t bgColor = hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str());
  tft->fillRect(barX + progressWidth, barY, barWidth - progressWidth, barHeight, bgColor);
}

void DisplayManager::clearAlarmOverlay()
{
  // This function needs to clear the area where "ALARM" and the progress bar are drawn.
  // We can define a bounding box that covers both elements.

  // The text is drawn with MC_DATUM (Middle Center)
  const int textY = (tft->height() / 2) + ALARM_TEXT_VISUAL_CENTER_VOFFSET;
  const int textHeight = 48; // Font 7 height

  // The progress bar is 30 pixels below the text's vertical center.
  const int barY = textY + 30;
  const int barHeight = 10;

  // Bounding box calculations
  const int clearY = textY - (textHeight / 2);
  const int clearHeight = (barY + barHeight) - clearY;

  // We can reuse the text width calculation for the clearing width, with some padding.
  const int clearWidth = tft->textWidth("ALARM", 7) + 20; // Add some padding
  const int clearX = (tft->width() / 2) - (clearWidth / 2);

  uint16_t bgColor = hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str());
  tft->fillRect(clearX, clearY, clearWidth, clearHeight, bgColor);
}

void DisplayManager::showErrorScreen(const char *message)
{
  tft->fillScreen(hexToRGB565(ConfigManager::getInstance().getBackgroundColor().c_str()));
  tft->setTextDatum(MC_DATUM);
  tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getErrorTextColor().c_str()));
  tft->drawString(message, tft->width() / 2, tft->height() / 2, 4); // Use font 4 for a clear message

  // Reset text datum and color for any potential subsequent drawing
  tft->setTextDatum(TL_DATUM);
  tft->setTextColor(hexToRGB565(ConfigManager::getInstance().getTimeColor().c_str()));
}