#pragma once

#include "Page.h"
#include "WeatherService.h"
#include "ConfigManager.h"

class WeatherPage : public Page
{
public:
  void onEnter(TFT_eSPI &tft) override;
  void onExit() override;
  void update() override;
  void render(TFT_eSPI &tft) override;
  void refresh(TFT_eSPI &tft, bool fullRefresh) override;

private:
  unsigned long _lastRenderTime = 0;
  WeatherData _lastWeatherData;
  bool _needsRedraw = true;

  // Helpers
  void drawWeather(TFT_eSPI &tft);
  uint16_t getConditionColor(const String &condition);
};
