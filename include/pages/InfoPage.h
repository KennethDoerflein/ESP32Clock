#pragma once

#include "Page.h"
#include <TFT_eSPI.h>

/**
 * @class InfoPage
 * @brief A simple page that displays static information.
 */
class InfoPage : public Page
{
public:
  InfoPage();
  virtual ~InfoPage();

  void onEnter(TFT_eSPI &tft) override;
  void onExit() override;
  void update() override;
  void render(TFT_eSPI &tft) override;
};