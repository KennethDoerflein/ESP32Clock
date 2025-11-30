#include "pages/WeatherPage.h"
#include "ConfigManager.h"
#include "DisplayManager.h"

#include "Utils.h"

void WeatherPage::onEnter(TFT_eSPI &tft)
{
  tft.fillScreen(hexToRGB565(ConfigManager::getInstance().getBackgroundColor()));
  tft.unloadFont(); // Ensure no custom fonts are loaded
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  _needsRedraw = true;
  _lastWeatherData.isValid = false; // Force update
}

void WeatherPage::onExit()
{
  // Cleanup if needed
}

void WeatherPage::update()
{
  // Check if weather data changed
  WeatherData current = WeatherService::getInstance().getCurrentWeather();
  if (current.isValid && (abs(current.temp - _lastWeatherData.temp) > 0.1 || current.condition != _lastWeatherData.condition))
  {
    _needsRedraw = true;
  }

  // Also redraw if Config changed (unit C/F)
  if (ConfigManager::getInstance().isDirty())
  {
    _needsRedraw = true;
  }
}

void WeatherPage::render(TFT_eSPI &tft)
{
  if (_needsRedraw)
  {
    drawWeather(tft);
    _needsRedraw = false;
    _lastWeatherData = WeatherService::getInstance().getCurrentWeather();
  }
}

void WeatherPage::refresh(TFT_eSPI &tft, bool fullRefresh)
{
  if (fullRefresh)
  {
    tft.fillScreen(hexToRGB565(ConfigManager::getInstance().getBackgroundColor()));
  }
  _needsRedraw = true;
  render(tft);
}

void WeatherPage::drawWeather(TFT_eSPI &tft)
{
  // Reset text size to default to avoid scaling issues from previous pages
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM); // Reset datum to default or what we use

  // Simple text based rendering for now

  uint16_t bg = hexToRGB565(ConfigManager::getInstance().getBackgroundColor());
  uint16_t txtColor = hexToRGB565(ConfigManager::getInstance().getWeatherTempColor());

  tft.fillScreen(bg);

  WeatherData data = WeatherService::getInstance().getCurrentWeather();

  if (!data.isValid)
  {
    tft.setTextColor(hexToRGB565(ConfigManager::getInstance().getErrorTextColor()), bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No Weather Data", tft.width() / 2, tft.height() / 2, 4);

    String zip = ConfigManager::getInstance().getZipCode();
    if (zip.length() == 0)
    {
      tft.drawString("Set Zip Code", tft.width() / 2, tft.height() / 2 + 30, 2);
    }
    else
    {
      tft.drawString("Updating...", tft.width() / 2, tft.height() / 2 + 30, 2);
    }
    return;
  }

  // Temperature
  float temp = data.temp;
  String unit = "F";
  if (ConfigManager::getInstance().isCelsius())
  {
    temp = (temp - 32.0) * 5.0 / 9.0;
    unit = "C";
  }

  String tempStr = String(temp, 1);

  tft.setTextColor(txtColor, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(7);

  int yPos = tft.height() / 2 - 75;
  tft.drawString(tempStr, tft.width() / 2, yPos, 7);

  int tempWidth = tft.textWidth(tempStr, 7);
  int unitX = (tft.width() / 2) + (tempWidth / 2) + 8; // Padding

  // Draw degree circle manually to match WeatherClockPage style
  int circleRadius = 3;
  int degreeX = unitX - circleRadius;
  int degreeY = yPos - 20;

  tft.fillCircle(degreeX, degreeY, circleRadius, txtColor);

  // Draw Unit Text
  int unitTextX = degreeX + circleRadius + 4;
  int unitTextY = yPos - 25;

  tft.setTextFont(4);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(unit, unitTextX, unitTextY, 4);

  // Condition
  tft.setTextFont(4);
  tft.setTextDatum(MC_DATUM); // Reset datum to center
  tft.setTextColor(hexToRGB565(ConfigManager::getInstance().getWeatherForecastColor()), bg);

  String condition = data.condition;
  tft.drawString(condition, tft.width() / 2, yPos + 55, 4);

  // Extended Data Grid
  // Use Forecast color for these labels too as they are part of weather info
  tft.setTextColor(hexToRGB565(ConfigManager::getInstance().getWeatherForecastColor()), bg);

  int gridY = yPos + 95;
  int leftColX = tft.width() / 4;
  int rightColX = tft.width() * 3 / 4;
  int rowSpacing = 55;

  float feelsLike = data.feelsLike;
  float windSpeed = data.windSpeed;
  String windUnit = "mph";
  if (ConfigManager::getInstance().isCelsius())
  {
    feelsLike = (feelsLike - 32.0) * 5.0 / 9.0;
    windSpeed = windSpeed * 1.60934;
    windUnit = "km/h";
  }

  // Row 1: Feels Like & Humidity
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Feels Like", leftColX, gridY, 4);
  tft.drawString(String(feelsLike, 1) + unit, leftColX, gridY + 25, 4);

  tft.drawString("Humidity", rightColX, gridY, 4);
  tft.drawString(String(data.humidity, 0) + "%", rightColX, gridY + 25, 4);

  // Row 2: Wind & Pressure
  gridY += rowSpacing;

  tft.drawString("Wind", leftColX, gridY, 4);
  tft.drawString(String(windSpeed, 1) + " " + windUnit, leftColX, gridY + 25, 4);

  tft.drawString("Pressure", rightColX, gridY, 4);
  tft.drawString(String(data.pressure, 0) + " hPa", rightColX, gridY + 25, 4);

  // Location
  tft.setTextDatum(MC_DATUM);
  tft.drawString(ConfigManager::getInstance().getZipCode(), tft.width() / 2, 20, 2);

  // Attribution
  tft.setTextColor(tft.color565(100, 100, 100), bg); // Dim gray
  tft.drawString("Weather data provided by open-meteo.com", tft.width() / 2, tft.height() - 15, 2);
}
