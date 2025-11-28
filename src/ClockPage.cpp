/**
 * @file ClockPage.cpp
 * @brief Implements the ClockPage class for displaying the main clock face.
 *
 * This file contains the implementation of the ClockPage, which is responsible
 * for rendering the time, date, sensor data, and alarm indicators. It uses
 * sprites for efficient updates and to prevent flickering.
 */

// Comment out the next line to disable sprite borders for debugging
// #define DEBUG_BORDERS

#include "pages/ClockPage.h"
#include "Constants.h"
#include "Utils.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "SensorModule.h"
#include "AlarmManager.h"
#include "fonts/DSEG7ModernBold104.h"
#include "fonts/DSEG14ModernBold32.h"
#include "fonts/DSEG14ModernBold48.h"
#include "fonts/DSEG7ModernBold48.h"
#include "fonts/CenturyGothicBold48.h"

#include <Arduino.h>

/**
 * @brief Constructs a new ClockPage.
 *
 * Initializes all the sprites used for rendering different parts of the clock face.
 * The TFT_eSPI driver instance is passed to each sprite's constructor.
 *
 * @param tft A pointer to the TFT_eSPI driver instance.
 */
ClockPage::ClockPage(TFT_eSPI *tft)
    : _sprClock(tft), _sprDayOfWeek(tft), _sprDate(tft), _sprTemp(tft), _sprHumidity(tft), _sprTOD(tft), _sprSeconds(tft),
      _alarmSprite(tft), _sprNextAlarm1(tft), _sprNextAlarm2(tft), _tft(tft)
{
  // Sprites are initialized in the member initializer list
}

/**
 * @brief Destroys the ClockPage.
 *
 * The sprites, being stack-allocated objects with a reference to the TFT driver,
 * are automatically destructed. No manual memory management is needed.
 */
ClockPage::~ClockPage()
{
  // Sprites with TFT_eSPI driver reference are destructed automatically
}

/**
 * @brief Called when the page becomes the active view.
 *
 * This method sets up the display for the clock page. It creates the sprites
 * on the first entry, recalculates the layout, and clears the screen.
 * It also forces a full redraw of all elements by clearing the last known data.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::onEnter(TFT_eSPI &tft)
{
  updateSpriteColors();
  tft.fillScreen(_bgColor);

  if (!_spritesCreated)
  {
    setupSprites(tft);
    initAlarmSprite(tft);
    _spritesCreated = true;
  }
  setupLayout(tft);

  // Force a redraw of all elements
  _lastData = {};
}

/**
 * @brief Called when the page is no longer the active view.
 *
 * This method is currently a no-op, as sprites are preserved between page
 * switches to improve performance.
 */
void ClockPage::onExit()
{
  // Sprites are not deleted on exit to preserve them.
}

/**
 * @brief Updates the internal state of the page.
 *
 * This method is currently a no-op, as all update logic is handled within
 * the `render` method for simplicity.
 */
void ClockPage::update()
{
  // The rendering logic handles this currently.
}

/**
 * @brief Renders the clock face to the display.
 *
 * This is the core rendering loop for the clock page. It fetches the current
 * display data and compares it to the last rendered data. If any data has
 * changed, it calls the appropriate draw function to update only the
 * necessary parts of the screen.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::render(TFT_eSPI &tft)
{
  DisplayData currentData;
  updateDisplayData(currentData);

  if (currentData.seconds != _lastData.seconds)
  {
    drawSeconds(tft);
    _lastData.seconds = currentData.seconds;
  }
  if (fabs(currentData.temp - _lastData.temp) > 0.1)
  {
    drawTemperature(tft);
    _lastData.temp = currentData.temp;
  }
  if (fabs(currentData.humidity - _lastData.humidity) > 0.1)
  {
    drawHumidity(tft);
    _lastData.humidity = currentData.humidity;
  }
  if (currentData.time != _lastData.time || currentData.tod != _lastData.tod)
  {
    drawClock(tft);
    _lastData.time = currentData.time;
    _lastData.tod = currentData.tod;
  }
  if (currentData.date != _lastData.date)
  {
    drawDate(tft);
    _lastData.date = currentData.date;
  }
  if (currentData.dayOfWeek != _lastData.dayOfWeek)
  {
    drawDayOfWeek(tft);
    _lastData.dayOfWeek = currentData.dayOfWeek;
  }
  if (currentData.nextAlarm1 != _lastData.nextAlarm1 || currentData.nextAlarm2 != _lastData.nextAlarm2)
  {
    drawNextAlarms(tft, currentData.nextAlarm1, currentData.nextAlarm2);
    _lastData.nextAlarm1 = currentData.nextAlarm1;
    _lastData.nextAlarm2 = currentData.nextAlarm2;
  }

  // Handle Alarm Sprite visibility
  auto &alarmManager = AlarmManager::getInstance();
  auto &config = ConfigManager::getInstance();
  bool isAlarmActive = alarmManager.isRinging() || config.isAnyAlarmSnoozed();

  if (isAlarmActive)
  {
    updateAlarmSprite();
    _wasAlarmActive = true;
  }
  else if (_wasAlarmActive)
  {
    // Transitioned from active to inactive.
    // Clear the alarm sprite area and force a redraw of the elements behind it (Next Alarms).
    clearAlarmSprite();
    drawNextAlarms(tft, currentData.nextAlarm1, currentData.nextAlarm2);
    _wasAlarmActive = false;
  }
}

/**
 * @brief Calculates the positions of all sprites on the screen.
 *
 * This method computes the coordinates for each element of the clock face
 * based on the screen dimensions and predefined margins. It ensures that the
 * layout is responsive to different screen sizes and that elements are
 * centered and aligned correctly.
 *
 * @param tft A reference to the TFT_eSPI driver instance, used to get screen dimensions.
 */
void ClockPage::setupLayout(TFT_eSPI &tft)
{
  int screenWidth = tft.width();
  int screenHeight = tft.height();

  // --- Clock Block Layout ---
  _clockY = MARGIN;

  // Calculate the width of the side elements column (AM/PM and seconds)
  int sideWidth = max(_sprTOD.width(), _sprSeconds.width());
  int timeSideGap = 15; // Gap between HH:MM and the side elements

  // Calculate the total width of the entire clock display
  int totalWidth = CLOCK_SPRITE_WIDTH + timeSideGap + sideWidth;

  // Calculate the starting X position to center the clock display
  int startX = (screenWidth - totalWidth) / 2;
  if (startX < 0)
    startX = 0;

  // Set the position for the main clock sprite
  _clockX = startX - 15;

  // Calculate the starting X for the side elements
  _sideX = _clockX + CLOCK_SPRITE_WIDTH + timeSideGap;

  // Calculate the total height of the side elements stack
  int sideElementsVGap = 15;
  int sideElementsHeight = TOD_SPRITE_HEIGHT + sideElementsVGap + SECONDS_SPRITE_HEIGHT;

  // Calculate the starting Y to vertically center the side elements against the main clock
  int sideStartY = _clockY + (CLOCK_SPRITE_HEIGHT - sideElementsHeight) / 2;
  if (sideStartY < _clockY)
    sideStartY = _clockY;

  // Set positions for AM/PM and Seconds sprites
  _todX = _sideX + sideElementsVGap;
  _todY = sideStartY;
  _secondsX = _todX + (TOD_SPRITE_WIDTH - SECONDS_SPRITE_WIDTH) / 2;
  _secondsY = _todY + TOD_SPRITE_HEIGHT + sideElementsVGap + 3;

  // --- Bottom Rows Layout (Date and Sensors) ---
  tft.loadFont(DSEG14ModernBold32);
  int fontHeight = tft.fontHeight();
  _alarmRowY = screenHeight - (fontHeight * 3 + MARGIN + 80);
  _dateY = screenHeight - (fontHeight * 2 + MARGIN + 40);
  _sensorY = screenHeight - (fontHeight + MARGIN + 10);

  // --- Alarm Sprite Layout ---
  _alarmSpriteX = (screenWidth - _alarmSprite.width()) / 2;
  _alarmSpriteY = (screenHeight / 2) - (_alarmSprite.height() / 2);
}

/**
 * @brief Clears the area of the screen where the alarm sprite is displayed.
 *
 * This is a helper function to manually clear the alarm sprite's footprint
 * on the screen, which can be useful when transitioning between alarm states.
 */
void ClockPage::clearAlarmSprite()
{
  _tft->fillRect(_alarmSpriteX, _alarmSpriteY, _alarmSprite.width(), _alarmSprite.height(), _bgColor);
}

/**
 * @brief Creates and configures all the sprites for the clock face.
 *
 * This method is called once on the first entry to the page. It allocates
 * memory for each sprite, loads the required fonts, and sets default text
 * alignment and colors.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::setupSprites(TFT_eSPI &tft)
{
  // Create the sprites and configure them
  _sprClock.createSprite(CLOCK_SPRITE_WIDTH, CLOCK_SPRITE_HEIGHT);
  _sprClock.loadFont(DSEG7ModernBold104);
  _sprClock.setTextDatum(MR_DATUM);

  _sprTOD.createSprite(TOD_SPRITE_WIDTH, TOD_SPRITE_HEIGHT);
  _sprTOD.loadFont(DSEG14ModernBold32);
  _sprTOD.setTextDatum(TR_DATUM);

  _sprSeconds.createSprite(SECONDS_SPRITE_WIDTH, SECONDS_SPRITE_HEIGHT);
  _sprSeconds.loadFont(DSEG7ModernBold48);
  _sprSeconds.setTextDatum(TR_DATUM);

  _sprDayOfWeek.createSprite(tft.width() / 2 - MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT);
  _sprDayOfWeek.loadFont(DSEG14ModernBold48);
  _sprDayOfWeek.setTextDatum(ML_DATUM);

  _sprDate.createSprite(tft.width() / 2 - MARGIN, DATE_SPRITE_HEIGHT);
  _sprDate.loadFont(DSEG14ModernBold48);
  _sprDate.setTextDatum(MR_DATUM);

  _sprNextAlarm1.createSprite(tft.width() / 2 - MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT); // Reusing height
  _sprNextAlarm1.loadFont(DSEG14ModernBold48);
  _sprNextAlarm1.setTextDatum(ML_DATUM);

  _sprNextAlarm2.createSprite(tft.width() / 2 - MARGIN, DAY_OF_WEEK_SPRITE_HEIGHT); // Reusing height
  _sprNextAlarm2.loadFont(DSEG14ModernBold48);
  _sprNextAlarm2.setTextDatum(MR_DATUM);

  _sprTemp.createSprite(tft.width() / 2 - MARGIN, TEMP_SPRITE_HEIGHT);
  _sprTemp.loadFont(DSEG14ModernBold48);
  _sprTemp.setTextDatum(ML_DATUM);

  _sprHumidity.createSprite(tft.width() / 2 - MARGIN, HUMIDITY_SPRITE_HEIGHT);
  _sprHumidity.loadFont(DSEG14ModernBold48);
  _sprHumidity.setTextDatum(MR_DATUM);

  updateSpriteColors();
}

/**
 * @brief Updates the colors of all sprites from the configuration.
 *
 * This method reads the latest color settings from the ConfigManager, converts
 * them from hex strings to RGB565 format, and applies them to the text
 * color of each sprite.
 */
void ClockPage::updateSpriteColors()
{
  auto &config = ConfigManager::getInstance();
  _bgColor = hexToRGB565(config.getBackgroundColor());
  uint16_t timeColor = hexToRGB565(config.getTimeColor());
  uint16_t todColor = hexToRGB565(config.getTodColor());
  uint16_t secondsColor = hexToRGB565(config.getSecondsColor());
  uint16_t dayOfWeekColor = hexToRGB565(config.getDayOfWeekColor());
  uint16_t dateColor = hexToRGB565(config.getDateColor());
  uint16_t tempColor = hexToRGB565(config.getTempColor());
  uint16_t humidityColor = hexToRGB565(config.getHumidityColor());
  uint16_t alarmColor = hexToRGB565(config.getAlarmTextColor());

  _sprClock.setTextColor(timeColor, _bgColor);
  _sprTOD.setTextColor(todColor, _bgColor);
  _sprSeconds.setTextColor(secondsColor, _bgColor);
  _sprDayOfWeek.setTextColor(dayOfWeekColor, _bgColor);
  _sprDate.setTextColor(dateColor, _bgColor);
  _sprTemp.setTextColor(tempColor, _bgColor);
  _sprHumidity.setTextColor(humidityColor, _bgColor);
  _sprNextAlarm1.setTextColor(alarmColor, _bgColor);
  _sprNextAlarm2.setTextColor(alarmColor, _bgColor);
}

/**
 * @brief Draws the main clock time (HH:MM) and AM/PM indicator.
 *
 * This function renders the current time into the clock sprite and pushes it
 * to the screen. If not in 24-hour format, it also renders the AM/PM
 * indicator into its own sprite.
 *
 * @param tft A reference to the TFT_eSPI driver instance (not directly used,
 *            but required by the Page interface).
 */
void ClockPage::drawClock(TFT_eSPI &tft)
{
  auto &timeManager = TimeManager::getInstance();
  bool is24Hour = timeManager.is24HourFormat();
  String timeStr = timeManager.getFormattedTime();
  String todStr = timeManager.getTOD();

  _sprClock.fillSprite(_bgColor);
  _sprClock.drawString(timeStr.c_str(), _sprClock.width(), _sprClock.height() / 2);
#ifdef DEBUG_BORDERS
  _sprClock.drawRect(0, 0, _sprClock.width(), _sprClock.height(), TFT_RED);
#endif
  _sprClock.pushSprite(_clockX, _clockY);

  if (!is24Hour)
  {
    _sprTOD.fillSprite(_bgColor);
    _sprTOD.drawString(todStr.c_str(), _sprTOD.width(), 0);
#ifdef DEBUG_BORDERS
    _sprTOD.drawRect(0, 0, _sprTOD.width(), _sprTOD.height(), TFT_GREEN);
#endif
    _sprTOD.pushSprite(_todX, _todY);
  }
}

/**
 * @brief Draws the seconds display.
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::drawSeconds(TFT_eSPI &tft)
{
  String secondsStr = TimeManager::getInstance().getFormattedSeconds();
  _sprSeconds.fillSprite(_bgColor);
  _sprSeconds.drawString(secondsStr.c_str(), _sprSeconds.width(), 0);
#ifdef DEBUG_BORDERS
  _sprSeconds.drawRect(0, 0, _sprSeconds.width(), _sprSeconds.height(), TFT_MAGENTA);
#endif
  _sprSeconds.pushSprite(_secondsX, _secondsY);
}

/**
 * @brief Draws the day of the week display.
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::drawDayOfWeek(TFT_eSPI &tft)
{
  String dayStr = TimeManager::getInstance().getDayOfWeek();
  _sprDayOfWeek.fillSprite(_bgColor);
  _sprDayOfWeek.drawString(dayStr.c_str(), 0, _sprDayOfWeek.height() / 2);
#ifdef DEBUG_BORDERS
  _sprDayOfWeek.drawRect(0, 0, _sprDayOfWeek.width(), _sprDayOfWeek.height(), TFT_BLUE);
#endif
  _sprDayOfWeek.pushSprite(MARGIN, _dateY);
}

/**
 * @brief Draws the date display.
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::drawDate(TFT_eSPI &tft)
{
  String dateStr = TimeManager::getInstance().getFormattedDate();
  _sprDate.fillSprite(_bgColor);
  _sprDate.drawString(dateStr.c_str(), _sprDate.width(), _sprDate.height() / 2);
#ifdef DEBUG_BORDERS
  _sprDate.drawRect(0, 0, _sprDate.width(), _sprDate.height(), TFT_YELLOW);
#endif
  _sprDate.pushSprite(tft.width() / 2, _dateY);
}

void ClockPage::drawNextAlarms(TFT_eSPI &tft, const String &alarm1, const String &alarm2)
{
  _sprNextAlarm1.fillSprite(_bgColor);
  if (alarm1.length() > 0)
  {
    _sprNextAlarm1.drawString(alarm1.c_str(), 0, _sprNextAlarm1.height() / 2);
  }
  _sprNextAlarm1.pushSprite(MARGIN, _alarmRowY);

  _sprNextAlarm2.fillSprite(_bgColor);
  if (alarm2.length() > 0)
  {
    _sprNextAlarm2.drawString(alarm2.c_str(), _sprNextAlarm2.width(), _sprNextAlarm2.height() / 2);
  }
  _sprNextAlarm2.pushSprite(tft.width() / 2, _alarmRowY);
}

/**
 * @brief Draws the temperature display.
 *
 * This function renders the temperature value, a degree symbol, and the
 * temperature unit (C/F) into the temperature sprite.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::drawTemperature(TFT_eSPI &tft)
{
  float temp = getTemperature();
  auto &config = ConfigManager::getInstance();
  uint16_t tempColor = hexToRGB565(config.getTempColor());

  _sprTemp.fillSprite(_bgColor);
  _sprTemp.loadFont(DSEG14ModernBold48);

  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", temp);
  _sprTemp.drawString(tempBuf, 0, _sprTemp.height() / 2);

  int tempWidth = _sprTemp.textWidth(tempBuf);
  int fontHeight = _sprTemp.fontHeight();
  int circleRadius = max(2, fontHeight / 14);
  int circleX = tempWidth + circleRadius + 2;
  int circleY = (_sprTemp.height() / 2) - (fontHeight / 2) + circleRadius;
  _sprTemp.fillCircle(circleX, circleY, circleRadius, tempColor);

  _sprTemp.loadFont(DSEG14ModernBold32);
  _sprTemp.setTextDatum(TL_DATUM);
  char unit = config.isCelsius() ? 'C' : 'F';
  char unitBuf[2] = {unit, '\0'};
  int unitX = circleX + circleRadius + 2;
  int unitY = (_sprTemp.height() / 2) - (fontHeight / 2);
  _sprTemp.drawString(unitBuf, unitX, unitY);
  _sprTemp.setTextDatum(ML_DATUM);

#ifdef DEBUG_BORDERS
  _sprTemp.drawRect(0, 0, _sprTemp.width(), _sprTemp.height(), TFT_ORANGE);
#endif
  _sprTemp.pushSprite(MARGIN, _sensorY);
}

/**
 * @brief Draws the humidity display.
 *
 * Renders the humidity value and a percent symbol into the humidity sprite.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::drawHumidity(TFT_eSPI &tft)
{
  float humidity = getHumidity();
  char buf[24];
  if (humidity < 0)
  {
    snprintf(buf, sizeof(buf), "N/A");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%.0f%%", humidity);
  }

  _sprHumidity.fillSprite(_bgColor);
  _sprHumidity.drawString(buf, _sprHumidity.width(), _sprHumidity.height() / 2);

#ifdef DEBUG_BORDERS
  _sprHumidity.drawRect(0, 0, _sprHumidity.width(), _sprHumidity.height(), TFT_CYAN);
#endif
  _sprHumidity.pushSprite(tft.width() / 2, _sensorY);
}

/**
 * @brief Populates a DisplayData struct with the latest data.
 *
 * This helper function gathers all the necessary information from the
 * TimeManager and SensorModule and stores it in the provided struct.
 * This is used by the `render` method to check for changes.
 *
 * @param data A reference to the DisplayData struct to be filled.
 */
void ClockPage::updateDisplayData(DisplayData &data)
{
  auto &timeManager = TimeManager::getInstance();
  data.time = timeManager.getFormattedTime();
  data.date = timeManager.getFormattedDate();
  data.dayOfWeek = timeManager.getDayOfWeek();
  data.temp = getTemperature();
  data.humidity = getHumidity();
  data.tod = timeManager.getTOD();
  data.seconds = timeManager.getFormattedSeconds();

  std::vector<NextAlarmTime> alarms = timeManager.getNextAlarms(2);
  bool is24Hour = timeManager.is24HourFormat();

  if (alarms.size() > 0)
  {
    char timeStr[8];
    if (is24Hour)
    {
      sprintf(timeStr, "%02d:%02d", alarms[0].time.hour(), alarms[0].time.minute());
    }
    else
    {
      int hour12 = alarms[0].time.hour() % 12;
      if (hour12 == 0)
        hour12 = 12;
      sprintf(timeStr, "%d:%02d", hour12, alarms[0].time.minute());
    }
    data.nextAlarm1 = String(timeStr);
  }
  else
  {
    data.nextAlarm1 = "";
  }

  if (alarms.size() > 1)
  {
    char timeStr[8];
    if (is24Hour)
    {
      sprintf(timeStr, "%02d:%02d", alarms[1].time.hour(), alarms[1].time.minute());
    }
    else
    {
      int hour12 = alarms[1].time.hour() % 12;
      if (hour12 == 0)
        hour12 = 12;
      sprintf(timeStr, "%d:%02d", hour12, alarms[1].time.minute());
    }
    data.nextAlarm2 = String(timeStr);
  }
  else
  {
    data.nextAlarm2 = "";
  }
}

/**
 * @brief Sets the progress of the alarm dismiss action.
 *
 * This is called from the main loop to update the visual progress bar when
 * the user is holding down the button to dismiss an alarm.
 *
 * @param progress The progress, from 0.0 (empty) to 1.0 (full).
 */
void ClockPage::setDismissProgress(float progress)
{
  _dismissProgress = progress;
}

/**
 * @brief Refreshes the display after settings have changed.
 *
 * This method is called when the configuration has been updated. It reapplies
 * colors and optionally clears the entire screen. It also forces a full
 * redraw of all elements on the next render pass.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 * @param fullRefresh If true, the entire screen is cleared.
 */
void ClockPage::refresh(TFT_eSPI &tft, bool fullRefresh)
{
  auto &config = ConfigManager::getInstance();
  updateSpriteColors();

  if (fullRefresh)
  {
    tft.fillScreen(_bgColor);
  }

  if (config.is24HourFormat())
  {
    // In 24-hour mode, ensure the TOD sprite is cleared immediately on refresh.
    _sprTOD.fillSprite(_bgColor);
    _sprTOD.pushSprite(_todX, _todY);
  }

  // Force a redraw of all elements by resetting their last known values
  _lastData = {};
}

/**
 * @brief Initializes the alarm sprite.
 *
 * Creates the sprite used for displaying "ALARM" or the snooze countdown.
 * It's sized based on the text width to be efficient.
 *
 * @param tft A reference to the TFT_eSPI driver instance.
 */
void ClockPage::initAlarmSprite(TFT_eSPI &tft)
{
  tft.loadFont(CenturyGothicBold48);
  int textWidth = tft.textWidth("ALARM");
  _alarmSprite.createSprite(textWidth + ALARM_SPRITE_WIDTH_PADDING, ALARM_SPRITE_HEIGHT);
  _alarmSprite.loadFont(CenturyGothicBold48);
  _alarmSprite.setTextDatum(MC_DATUM);
  _alarmSprite.setTextColor(hexToRGB565(ConfigManager::getInstance().getAlarmTextColor().c_str()));
  tft.unloadFont();
}

/**
 * @brief Updates and renders the alarm sprite.
 *
 * This function contains the logic to display the correct alarm state:
 * - "ALARM" when an alarm is ringing.
 * - A snooze countdown when an alarm is snoozed.
 * - A progress bar when the user is holding the dismiss button.
 * - Nothing when no alarm is active.
 *
 * The updated sprite is then pushed to the screen.
 */
void ClockPage::updateAlarmSprite()
{
  auto &config = ConfigManager::getInstance();
  uint16_t alarmColor = hexToRGB565(config.getAlarmTextColor().c_str());

  // Fill the sprite with the alarm color to create a solid box
  _alarmSprite.fillSprite(alarmColor);
  // Set text color to background color (inverted)
  _alarmSprite.setTextColor(_bgColor, alarmColor);

  auto &alarmManager = AlarmManager::getInstance();

  bool anySnoozed = false;
  for (int i = 0; i < config.getNumAlarms(); ++i)
  {
    if (config.getAlarm(i).isSnoozed())
    {
      anySnoozed = true;
      break;
    }
  }

  if (alarmManager.isRinging())
  {
    _alarmSprite.drawString("ALARM", _alarmSprite.width() / 2, _alarmSprite.height() / 2);
    if (_dismissProgress > 0.0f)
    {
      int barWidth = _alarmSprite.width() * _dismissProgress;
      // Use background color for progress bar since background is alarmColor
      _alarmSprite.fillRect(0, _alarmSprite.height() - ALARM_PROGRESS_BAR_HEIGHT, barWidth, ALARM_PROGRESS_BAR_HEIGHT, _bgColor);
    }
  }
  else if (anySnoozed)
  {
    bool foundSnoozed = false;
    for (int i = 0; i < config.getNumAlarms(); ++i)
    {
      const auto &alarm = config.getAlarm(i);
      if (alarm.isSnoozed())
      {
        time_t snoozeUntil = alarm.getSnoozeUntil();
        time_t now = TimeManager::getInstance().getRTCTime().unixtime();
        long remaining = snoozeUntil - now;
        if (remaining < 0)
        {
          remaining = 0;
        }

        char buf[10];
        snprintf(buf, sizeof(buf), "%ld:%02ld", remaining / 60, remaining % 60);
        _alarmSprite.drawString(buf, _alarmSprite.width() / 2, _alarmSprite.height() / 2);
        foundSnoozed = true;
        break;
      }
    }
    if (!foundSnoozed)
    {
      // This case is reached if the snooze was just cancelled.
      _alarmSprite.fillSprite(_bgColor);
    }

    // Also draw the progress bar if the button is being held
    if (_dismissProgress > 0.0f)
    {
      int barWidth = _alarmSprite.width() * _dismissProgress;
      _alarmSprite.fillRect(0, _alarmSprite.height() - ALARM_PROGRESS_BAR_HEIGHT, barWidth, ALARM_PROGRESS_BAR_HEIGHT, _bgColor);
    }
  }
  else
  {
    _dismissProgress = 0.0f;
  }

  _alarmSprite.pushSprite(_alarmSpriteX, _alarmSpriteY);
}
