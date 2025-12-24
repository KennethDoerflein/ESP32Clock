/**
 * @file InfoPage.cpp
 * @brief Implements the InfoPage class for displaying system information.
 *
 * This file contains the implementation for the InfoPage, which shows details
 * like the device's hostname, IP address, and the current firmware version.
 * The content is static and rendered only once upon entering the page.
 */

#include "pages/InfoPage.h"
#include "fonts/CenturyGothic28.h"
#include <WiFi.h>
#if __has_include("version.h")
// This file exists, so we'll include it.
#include "version.h"
#else
// "version.h" was not found, so we'll include the backup.
#include "version.h.default"
#endif

/**
 * @brief Constructs a new InfoPage.
 */
InfoPage::InfoPage() {}

/**
 * @brief Destroys the InfoPage.
 */
InfoPage::~InfoPage() {}

/**
 * @brief Called when the page becomes the active view.
 *
 * Renders the static information to the screen. This is done only once, as
 * the content does not change.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void InfoPage::onEnter(TFT_eSPI &tft)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.loadFont(CenturyGothic28);

  // --- Title ---
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Info", tft.width() / 2, 40);

  // --- Info ---
  tft.setTextDatum(ML_DATUM); // Left-align the info text
  int y_pos = 90;
  int x_pos = 20;

  // Display Hostname
  String hostname = "Host: ";
  hostname += WiFi.getHostname();
  hostname += ".local";
  tft.drawString(hostname, x_pos, y_pos);

  // Display IP Address
  y_pos += 30;
  String ip = "IP: ";
  ip += WiFi.localIP().toString();
  tft.drawString(ip, x_pos, y_pos);

  // Display Version
  y_pos += 30;
  String version = "Version: ";
  version += FIRMWARE_VERSION;
  tft.drawString(version, x_pos, y_pos);

  tft.unloadFont();
}

/**
 * @brief Called when the page is no longer the active view.
 *
 * This method is a no-op as there is no cleanup required for this simple page.
 */
void InfoPage::onExit()
{
  // Nothing to do here
}

/**
 * @brief Updates the internal state of the page.
 *
 * This method is a no-op because the page displays static information that
 * does not need to be updated in the main loop.
 */
void InfoPage::update()
{
  // Static content, nothing to do here
}

/**
 * @brief Renders the page to the display.
 *
 * This method is a no-op because all rendering is done in the `onEnter`
 * method. The content is static and does not need to be redrawn repeatedly.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void InfoPage::render(TFT_eSPI &tft)
{
  // Static content, no need for continuous rendering
}