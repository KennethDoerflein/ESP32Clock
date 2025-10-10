#include "pages/InfoPage.h"
#include "fonts/DSEG14ModernBold16.h"
#include <WiFi.h>

InfoPage::InfoPage() {}

InfoPage::~InfoPage() {}

void InfoPage::onEnter(TFT_eSPI &tft)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.loadFont(DSEG14ModernBold16);

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
  tft.drawString(hostname, x_pos, y_pos);

  // Display IP Address
  y_pos += 30;
  String ip = "IP: ";
  ip += WiFi.localIP().toString();
  tft.drawString(ip, x_pos, y_pos);

  // Display Version
  y_pos += 30;
  tft.drawString("Version: 0.0.0", x_pos, y_pos);

  tft.unloadFont();
}

void InfoPage::onExit()
{
  // Nothing to do here
}

void InfoPage::update()
{
  // Static content, nothing to do here
}

void InfoPage::render(TFT_eSPI &tft)
{
  // Static content, no need for continuous rendering
}