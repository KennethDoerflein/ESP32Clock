// ConfigManager.cpp

/**
 * @file ConfigManager.cpp
 * @brief Implements the ConfigManager class for handling application settings.
 *
 * This file contains the implementation for loading, saving, and managing all
 * configuration settings for the clock. It uses the ESP32's Preferences library
 * for persistent storage in non-volatile flash memory.
 */

#include "ConfigManager.h"
#include "SerialLog.h"
#include "nvs_flash.h"
#include "Constants.h"
#include "LockGuard.h"
#include "UpdateManager.h"

/**
 * @brief Private constructor to enforce the singleton pattern.
 */
ConfigManager::ConfigManager() : _isDirty(false), _savePending(false), _saveDebounceTimer(0), _nextAlarmId(0)
{
  _mutex = xSemaphoreCreateRecursiveMutex();
}

/**
 * @brief Handles periodic tasks for the ConfigManager, like debounced saves.
 *
 * This should be called in the main application loop. It checks if a save
 * operation is pending and executes it after a debounce delay, preventing
 * excessive writes to flash memory.
 */
void ConfigManager::loop()
{
  if (UpdateManager::getInstance().isUpdateInProgress())
    return;

  bool pending = false;
  {
    RecursiveLockGuard lock(_mutex);
    pending = _savePending && (millis() - _saveDebounceTimer >= SAVE_DEBOUNCE_DELAY);
  }

  if (pending)
  {
    save();
    RecursiveLockGuard lock(_mutex);
    _savePending = false;
  }
}

/**
 * @brief Schedules a configuration save.
 *
 * Sets a flag and a timer to save the configuration after a short delay.
 * This debouncing mechanism prevents rapid, successive writes to flash
 * when multiple settings are changed quickly via the web interface.
 */
void ConfigManager::scheduleSave()
{
  RecursiveLockGuard lock(_mutex);
  _savePending = true;
  _saveDebounceTimer = millis();
}

/**
 * @brief Initializes the ConfigManager.
 *
 * Opens the "clock_config" namespace in the Preferences library and then
 * calls `load()` to populate the configuration variables from flash memory.
 */
void ConfigManager::begin()
{
  _preferences.begin(PREFERENCES_NAMESPACE, false); // R/W mode
  // After ensuring the filesystem is ready, load the configuration.
  load();
}

/**
 * @brief Sets all configuration variables to their default values.
 *
 * This is used during a factory reset or on the very first boot to ensure
 * the device starts in a known, stable state.
 */
void ConfigManager::setDefaults()
{
  RecursiveLockGuard lock(_mutex);
  // Set all configuration parameters to their default initial state.
  wifiSSID = DEFAULT_WIFI_SSID;
  wifiPassword = DEFAULT_WIFI_PASSWORD;
  hostname = DEFAULT_HOSTNAME;
  wifiCredsValid = DEFAULT_WIFI_CREDS_VALID;
  ringingAlarmId = DEFAULT_RINGING_ALARM_ID;
  ringingAlarmStartTimestamp = DEFAULT_RINGING_ALARM_TIMESTAMP;
  autoBrightness = DEFAULT_AUTO_BRIGHTNESS;
  brightness = DEFAULT_BRIGHTNESS;
  autoBrightnessStartHour = DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
  autoBrightnessEndHour = DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
  dayBrightness = DEFAULT_DAY_BRIGHTNESS;
  nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
  use24HourFormat = DEFAULT_USE_24_HOUR_FORMAT;
  useCelsius = DEFAULT_USE_CELSIUS;
  screenFlipped = DEFAULT_SCREEN_FLIPPED;
  invertColors = DEFAULT_INVERT_COLORS;
  timezone = DEFAULT_TIMEZONE;
  setenv("TZ", timezone.c_str(), 1);
  tzset();
  isDst = DEFAULT_IS_DST;
  snoozeDuration = DEFAULT_SNOOZE_DURATION;
  dismissDuration = DEFAULT_DISMISS_DURATION;
  tempCorrectionEnabled = DEFAULT_TEMP_CORRECTION_ENABLED;
  tempCorrection = DEFAULT_TEMP_CORRECTION;
  address = DEFAULT_ADDRESS;
  enabledPages.assign(std::begin(DEFAULT_ENABLED_PAGES), std::end(DEFAULT_ENABLED_PAGES));
  defaultPage = DEFAULT_DEFAULT_PAGE;
  lat = DEFAULT_LAT;
  lon = DEFAULT_LON;

  backgroundColor = DEFAULT_BACKGROUND_COLOR;
  timeColor = DEFAULT_TIME_COLOR;
  todColor = DEFAULT_TOD_COLOR;
  secondsColor = DEFAULT_SECONDS_COLOR;
  dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  dateColor = DEFAULT_DATE_COLOR;
  tempColor = DEFAULT_TEMP_COLOR;
  humidityColor = DEFAULT_HUMIDITY_COLOR;
  alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  errorTextColor = DEFAULT_ERROR_TEXT_COLOR;
  weatherTempColor = DEFAULT_WEATHER_TEMP_COLOR;
  weatherForecastColor = DEFAULT_WEATHER_FORECAST_COLOR;

  _alarms.clear();
  for (int i = 0; i < DEFAULT_ALARMS_COUNT; ++i)
  {
    Alarm alarm;
    alarm.setId(i);
    _alarms.push_back(alarm);
  }
  _nextAlarmId = DEFAULT_ALARMS_COUNT;

  SerialLog::getInstance().print("Loaded default configuration.");
}

/**
 * @brief Loads the configuration from persistent storage (Preferences).
 *
 * Checks for a "first boot" flag. If it's the first time the device has
 * run, it loads the default settings and saves them. Otherwise, it reads
 * each configuration value from flash memory.
 */
void ConfigManager::load()
{
  // Check if this is the first boot
  bool firstBoot = !_preferences.getBool("firstBootDone", false);
  if (firstBoot)
  {
    SerialLog::getInstance().print("First boot detected. Loading default configuration.");
    setDefaults();
    save(); // Save defaults to preferences
    _preferences.putBool("firstBootDone", true);
    return;
  }

  SerialLog::getInstance().print("Loading configuration from Preferences...");

  RecursiveLockGuard lock(_mutex);
  // Load each setting from the Preferences
  wifiSSID = _preferences.getString("wifiSSID", DEFAULT_WIFI_SSID);
  wifiPassword = _preferences.getString("wifiPass", DEFAULT_WIFI_PASSWORD);
  hostname = _preferences.getString("hostname", DEFAULT_HOSTNAME);
  wifiCredsValid = _preferences.getBool("wifiValid", DEFAULT_WIFI_CREDS_VALID);
  ringingAlarmId = _preferences.getChar("ringAlarmId", DEFAULT_RINGING_ALARM_ID);
  ringingAlarmStartTimestamp = _preferences.getUInt("ringAlarmTS", DEFAULT_RINGING_ALARM_TIMESTAMP);
  autoBrightness = _preferences.getBool("autoBright", DEFAULT_AUTO_BRIGHTNESS);
  brightness = _preferences.getUChar("brightness", DEFAULT_BRIGHTNESS);
  autoBrightnessStartHour = _preferences.getUChar("autoBrStartHr", DEFAULT_AUTO_BRIGHTNESS_START_HOUR);
  autoBrightnessEndHour = _preferences.getUChar("autoBrEndHr", DEFAULT_AUTO_BRIGHTNESS_END_HOUR);
  dayBrightness = _preferences.getUChar("dayBright", DEFAULT_DAY_BRIGHTNESS);
  nightBrightness = _preferences.getUChar("nightBright", DEFAULT_NIGHT_BRIGHTNESS);
  use24HourFormat = _preferences.getBool("is24Hour", DEFAULT_USE_24_HOUR_FORMAT);
  useCelsius = _preferences.getBool("useCelsius", DEFAULT_USE_CELSIUS);
  screenFlipped = _preferences.getBool("screenFlip", DEFAULT_SCREEN_FLIPPED);
  invertColors = _preferences.getBool("invertColors", DEFAULT_INVERT_COLORS);
  timezone = _preferences.getString("timezone", DEFAULT_TIMEZONE);
  setenv("TZ", timezone.c_str(), 1);
  tzset();
  isDst = _preferences.getBool("isDst", DEFAULT_IS_DST);
  snoozeDuration = _preferences.getUChar("snoozeDur", DEFAULT_SNOOZE_DURATION);
  dismissDuration = _preferences.getUChar("dismissDur", DEFAULT_DISMISS_DURATION);
  tempCorrectionEnabled = _preferences.getBool("tempCorrEn", DEFAULT_TEMP_CORRECTION_ENABLED);
  tempCorrection = _preferences.getFloat("tempCorr", DEFAULT_TEMP_CORRECTION);

  // Try to load address, fall back to zipCode for migration
  address = _preferences.getString("address", "");
  if (address.isEmpty())
  {
    address = _preferences.getString("zipCode", DEFAULT_ADDRESS);
  }

  String pagesStr = _preferences.getString("pageOrder", "");
  enabledPages.clear();
  if (pagesStr.length() > 0)
  {
    int start = 0;
    int end = pagesStr.indexOf(',');
    while (end != -1)
    {
      enabledPages.push_back(pagesStr.substring(start, end).toInt());
      start = end + 1;
      end = pagesStr.indexOf(',', start);
    }
    enabledPages.push_back(pagesStr.substring(start).toInt());
  }
  else
  {
    enabledPages.assign(std::begin(DEFAULT_ENABLED_PAGES), std::end(DEFAULT_ENABLED_PAGES));
  }

  defaultPage = _preferences.getInt("defaultPage", DEFAULT_DEFAULT_PAGE);
  lat = _preferences.getFloat("lat", DEFAULT_LAT);
  lon = _preferences.getFloat("lon", DEFAULT_LON);

  backgroundColor = _preferences.getString("bgClr", DEFAULT_BACKGROUND_COLOR);
  timeColor = _preferences.getString("timeClr", DEFAULT_TIME_COLOR);
  todColor = _preferences.getString("todClr", DEFAULT_TOD_COLOR);
  secondsColor = _preferences.getString("secondsClr", DEFAULT_SECONDS_COLOR);
  dayOfWeekColor = _preferences.getString("dayOfWeekClr", DEFAULT_DAY_OF_WEEK_COLOR);
  dateColor = _preferences.getString("dateClr", DEFAULT_DATE_COLOR);
  tempColor = _preferences.getString("tempClr", DEFAULT_TEMP_COLOR);
  humidityColor = _preferences.getString("humidityClr", DEFAULT_HUMIDITY_COLOR);
  alarmIconColor = _preferences.getString("alarmIconClr", DEFAULT_ALARM_ICON_COLOR);
  snoozeIconColor = _preferences.getString("snzIconClr", DEFAULT_SNOOZE_ICON_COLOR);
  alarmTextColor = _preferences.getString("alarmTextClr", DEFAULT_ALARM_TEXT_COLOR);
  errorTextColor = _preferences.getString("errorTextClr", DEFAULT_ERROR_TEXT_COLOR);
  weatherTempColor = _preferences.getString("weaTempClr", DEFAULT_WEATHER_TEMP_COLOR);
  weatherForecastColor = _preferences.getString("weaFcstClr", DEFAULT_WEATHER_FORECAST_COLOR);

  // Validate colors to prevent issues with URL-encoded values
  if (backgroundColor.startsWith("%"))
    backgroundColor = DEFAULT_BACKGROUND_COLOR;
  if (timeColor.startsWith("%"))
    timeColor = DEFAULT_TIME_COLOR;
  if (todColor.startsWith("%"))
    todColor = DEFAULT_TOD_COLOR;
  if (secondsColor.startsWith("%"))
    secondsColor = DEFAULT_SECONDS_COLOR;
  if (dayOfWeekColor.startsWith("%"))
    dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
  if (dateColor.startsWith("%"))
    dateColor = DEFAULT_DATE_COLOR;
  if (tempColor.startsWith("%"))
    tempColor = DEFAULT_TEMP_COLOR;
  if (humidityColor.startsWith("%"))
    humidityColor = DEFAULT_HUMIDITY_COLOR;
  if (alarmIconColor.startsWith("%"))
    alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
  if (snoozeIconColor.startsWith("%"))
    snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  if (alarmTextColor.startsWith("%"))
    alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
  if (errorTextColor.startsWith("%"))
    errorTextColor = DEFAULT_ERROR_TEXT_COLOR;
  if (weatherTempColor.startsWith("%"))
    weatherTempColor = DEFAULT_WEATHER_TEMP_COLOR;
  if (weatherForecastColor.startsWith("%"))
    weatherForecastColor = DEFAULT_WEATHER_FORECAST_COLOR;

  // Load alarms
  _alarms.clear();
  int numAlarms = _preferences.getInt("numAlarms", -1);

  if (numAlarms == -1)
  {
    // Legacy support: Assume 5 fixed alarms
    for (int i = 0; i < LEGACY_ALARMS_COUNT; ++i)
    {
      Alarm alarm;
      String prefix = "a_" + String(i) + "_";
      alarm.setId(i);
      alarm.setEnabled(_preferences.getBool((prefix + "en").c_str(), false));
      alarm.setHour(_preferences.getUChar((prefix + "hr").c_str(), 6));
      alarm.setMinute(_preferences.getUChar((prefix + "min").c_str(), 0));
      alarm.setDays(_preferences.getUChar((prefix + "days").c_str(), 0));
      bool snoozed = _preferences.getBool((prefix + "snz").c_str(), false);
      uint32_t snoozeUntil = _preferences.getUInt((prefix + "snzUntil").c_str(), 0);
      alarm.setSnoozeState(snoozed, snoozeUntil);
      alarm.setLastDismissedDay(_preferences.getUChar((prefix + "lastDis").c_str(), 8));
      _alarms.push_back(alarm);
    }
    _nextAlarmId = LEGACY_ALARMS_COUNT;
  }
  else
  {
    // Dynamic alarms
    _nextAlarmId = _preferences.getInt("nextAlarmId", 0);
    for (int i = 0; i < numAlarms; ++i)
    {
      Alarm alarm;
      String prefix = "a_" + String(i) + "_";
      alarm.setId(_preferences.getUChar((prefix + "id").c_str(), 0));
      alarm.setEnabled(_preferences.getBool((prefix + "en").c_str(), false));
      alarm.setHour(_preferences.getUChar((prefix + "hr").c_str(), 6));
      alarm.setMinute(_preferences.getUChar((prefix + "min").c_str(), 0));
      alarm.setDays(_preferences.getUChar((prefix + "days").c_str(), 0));
      bool snoozed = _preferences.getBool((prefix + "snz").c_str(), false);
      uint32_t snoozeUntil = _preferences.getUInt((prefix + "snzUntil").c_str(), 0);
      alarm.setSnoozeState(snoozed, snoozeUntil);
      alarm.setLastDismissedDay(_preferences.getUChar((prefix + "lastDis").c_str(), 8));
      _alarms.push_back(alarm);
    }
  }

  SerialLog::getInstance().print("Configuration loaded successfully.");
}

/**
 * @brief Saves the current configuration to persistent storage.
 *
 * Writes all configuration variables to flash memory using the Preferences
 * library.
 *
 * @return True on success, false on failure (though Preferences does not
 *         currently indicate failure).
 */
bool ConfigManager::save()
{
  if (UpdateManager::getInstance().isUpdateInProgress())
    return false;

  RecursiveLockGuard lock(_mutex);
  _preferences.putString("wifiSSID", wifiSSID);
  _preferences.putString("wifiPass", wifiPassword);
  _preferences.putString("hostname", hostname);
  _preferences.putBool("wifiValid", wifiCredsValid);
  _preferences.putChar("ringAlarmId", ringingAlarmId);
  _preferences.putUInt("ringAlarmTS", ringingAlarmStartTimestamp);
  _preferences.putBool("autoBright", autoBrightness);
  _preferences.putUChar("brightness", brightness);
  _preferences.putUChar("autoBrStartHr", autoBrightnessStartHour);
  _preferences.putUChar("autoBrEndHr", autoBrightnessEndHour);
  _preferences.putUChar("dayBright", dayBrightness);
  _preferences.putUChar("nightBright", nightBrightness);
  _preferences.putBool("is24Hour", use24HourFormat);
  _preferences.putBool("useCelsius", useCelsius);
  _preferences.putBool("screenFlip", screenFlipped);
  _preferences.putBool("invertColors", invertColors);
  _preferences.putString("timezone", timezone);
  _preferences.putBool("isDst", isDst);
  _preferences.putUChar("snoozeDur", snoozeDuration);
  _preferences.putUChar("dismissDur", dismissDuration);
  _preferences.putBool("tempCorrEn", tempCorrectionEnabled);
  _preferences.putFloat("tempCorr", tempCorrection);
  _preferences.putString("address", address);

  String pagesStr = "";
  for (size_t i = 0; i < enabledPages.size(); ++i)
  {
    pagesStr += String(enabledPages[i]);
    if (i < enabledPages.size() - 1)
      pagesStr += ",";
  }
  _preferences.putString("pageOrder", pagesStr);

  _preferences.putInt("defaultPage", defaultPage);
  _preferences.putFloat("lat", lat);
  _preferences.putFloat("lon", lon);

  _preferences.putString("bgClr", backgroundColor);
  _preferences.putString("timeClr", timeColor);
  _preferences.putString("todClr", todColor);
  _preferences.putString("secondsClr", secondsColor);
  _preferences.putString("dayOfWeekClr", dayOfWeekColor);
  _preferences.putString("dateClr", dateColor);
  _preferences.putString("tempClr", tempColor);
  _preferences.putString("humidityClr", humidityColor);
  _preferences.putString("alarmIconClr", alarmIconColor);
  _preferences.putString("snzIconClr", snoozeIconColor);
  _preferences.putString("alarmTextClr", alarmTextColor);
  _preferences.putString("errorTextClr", errorTextColor);
  _preferences.putString("weaTempClr", weatherTempColor);
  _preferences.putString("weaFcstClr", weatherForecastColor);

  // Save alarms
  _preferences.putInt("numAlarms", _alarms.size());
  _preferences.putInt("nextAlarmId", _nextAlarmId);

  for (size_t i = 0; i < _alarms.size(); ++i)
  {
    String prefix = "a_" + String(i) + "_";
    _preferences.putUChar((prefix + "id").c_str(), _alarms[i].getId());
    _preferences.putBool((prefix + "en").c_str(), _alarms[i].isEnabled());
    _preferences.putUChar((prefix + "hr").c_str(), _alarms[i].getHour());
    _preferences.putUChar((prefix + "min").c_str(), _alarms[i].getMinute());
    _preferences.putUChar((prefix + "days").c_str(), _alarms[i].getDays());
    _preferences.putBool((prefix + "snz").c_str(), _alarms[i].isSnoozed());
    _preferences.putUInt((prefix + "snzUntil").c_str(), _alarms[i].getSnoozeUntil());
    _preferences.putUChar((prefix + "lastDis").c_str(), _alarms[i].getLastDismissedDay());
  }

  SerialLog::getInstance().print("Configuration saved.");
  return true;
}

/**
 * @brief Saves only the state of a ringing alarm.
 *
 * This is a specialized, lightweight save function used to quickly persist
 * which alarm is ringing and when it started. This ensures that if the
 * device reboots, it can resume the alarm correctly.
 */
void ConfigManager::saveRingingAlarmState()
{
  RecursiveLockGuard lock(_mutex);
  _preferences.putChar("ringAlarmId", ringingAlarmId);
  _preferences.putUInt("ringAlarmTS", ringingAlarmStartTimestamp);
}

/**
 * @brief Gets a copy of a specific alarm by index.
 * @param index The index of the alarm.
 * @return An Alarm object.
 */
Alarm ConfigManager::getAlarmByIndex(int index) const
{
  RecursiveLockGuard lock(_mutex);
  if (index < 0 || index >= _alarms.size())
  {
    SerialLog::getInstance().print("FATAL: Alarm index out of bounds!");
    // Return a default alarm or handle error appropriately.
    // Since we can't crash safely, returning a disabled alarm.
    return Alarm();
  }
  return _alarms[index];
}

/**
 * @brief Gets a copy of a specific alarm by ID.
 * @param id The unique ID of the alarm.
 * @return An Alarm object. If not found, returns an invalid alarm (check via ID or enabled status if applicable).
 */
Alarm ConfigManager::getAlarmById(int id) const
{
  RecursiveLockGuard lock(_mutex);
  for (const auto &alarm : _alarms)
  {
    if (alarm.getId() == id)
    {
      return alarm;
    }
  }

  Alarm notFound;
  notFound.setId(255); // Sentinel
  return notFound;
}

/**
 * @brief Gets the total number of alarms supported.
 * @return The number of alarms.
 */
int ConfigManager::getNumAlarms() const
{
  RecursiveLockGuard lock(_mutex);
  return _alarms.size();
}

/**
 * @brief Updates an alarm's configuration by index.
 * @param index The index of the alarm to update.
 * @param alarm The new alarm data to set.
 */
void ConfigManager::setAlarmByIndex(int index, const Alarm &alarm)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (index < 0 || index >= _alarms.size())
    {
      SerialLog::getInstance().print("ERROR: Alarm index out of bounds!");
      return;
    }
    _alarms[index] = alarm;
    _isDirty = true;
  }
  scheduleSave();
}

/**
 * @brief Updates an alarm's configuration by ID.
 * @param id The ID of the alarm to update.
 * @param alarm The new alarm data to set.
 */
void ConfigManager::setAlarmById(int id, const Alarm &alarm)
{
  bool found = false;
  {
    RecursiveLockGuard lock(_mutex);
    for (auto &a : _alarms)
    {
      if (a.getId() == id)
      {
        a = alarm;
        _isDirty = true;
        found = true;
        break;
      }
    }
  }
  if (found)
    scheduleSave();
  else
    SerialLog::getInstance().printf("ERROR: Alarm ID %d not found for update!\n", id);
}

void ConfigManager::replaceAlarms(const std::vector<Alarm> &newAlarms)
{
  {
    RecursiveLockGuard lock(_mutex);
    _alarms.clear();
    for (const auto &incomingAlarm : newAlarms)
    {
      Alarm alarm = incomingAlarm;
      // If the ID is invalid (new alarm from frontend), assign a new ID
      if (alarm.getId() == 255 || alarm.getId() < 0)
      { // Using uint8_t, 255 might be -1 casted, or we check against known invalid
        // Assuming frontend sends ID as int, but Alarm stores as uint8_t.
        // If frontend sends -1, it becomes 255 in uint8_t
        alarm.setId(_nextAlarmId++);
      }
      _alarms.push_back(alarm);
    }
    _isDirty = true;
  }
  scheduleSave();
}

/**
 * @brief Performs a full factory reset.
 *
 * This function erases all configuration settings, including saved WiFi
 * credentials, by clearing the entire Preferences namespace and erasing the
 * NVS partition. The device is then reset to its default state.
 */
void ConfigManager::factoryReset()
{
  SerialLog::getInstance().print("Performing factory reset...\n");

  // Erase the NVS (Non-Volatile Storage) partition.
  // This is where the WiFi credentials are saved by the WiFi library.
  SerialLog::getInstance().print("Erasing NVS to clear WiFi credentials...\n");
  esp_err_t err = nvs_flash_erase();
  if (err == ESP_OK)
  {
    SerialLog::getInstance().print("NVS erased successfully.\n");
  }
  else
  {
    SerialLog::getInstance().print("Error erasing NVS.\n");
  }

  // After erasing, the NVS needs to be re-initialized for the next boot.
  err = nvs_flash_init();
  if (err == ESP_OK)
  {
    SerialLog::getInstance().print("NVS re-initialized successfully.\n");
  }
  else
  {
    SerialLog::getInstance().print("Error re-initializing NVS.\n");
  }

  setDefaults();
  save();
}

/**
 * @brief Performs a factory reset while preserving WiFi credentials.
 *
 * This function resets all settings to their default values but first reads
 * and then restores the existing WiFi SSID, password, and validity status.
 * This is useful for resetting the clock's settings without needing to
 * re-configure the network connection.
 */
void ConfigManager::factoryResetExceptWiFi()
{
  SerialLog::getInstance().print("Performing factory reset, but keeping WiFi credentials...\n");

  // Preserve WiFi credentials
  String ssid = getWifiSSID();
  String password = getWifiPassword();
  bool credsValid = areWifiCredsValid();

  setDefaults();

  // Restore WiFi credentials
  setWifiSSID(ssid);
  setWifiPassword(password);
  setWifiCredsValid(credsValid);

  save();
}

/**
 * @brief Resets only the display color settings to their default values.
 */
void ConfigManager::resetDisplayToDefaults()
{
  {
    RecursiveLockGuard lock(_mutex);
    backgroundColor = DEFAULT_BACKGROUND_COLOR;
    timeColor = DEFAULT_TIME_COLOR;
    todColor = DEFAULT_TOD_COLOR;
    secondsColor = DEFAULT_SECONDS_COLOR;
    dayOfWeekColor = DEFAULT_DAY_OF_WEEK_COLOR;
    dateColor = DEFAULT_DATE_COLOR;
    tempColor = DEFAULT_TEMP_COLOR;
    humidityColor = DEFAULT_HUMIDITY_COLOR;
    alarmIconColor = DEFAULT_ALARM_ICON_COLOR;
    alarmTextColor = DEFAULT_ALARM_TEXT_COLOR;
    errorTextColor = DEFAULT_ERROR_TEXT_COLOR;
    weatherTempColor = DEFAULT_WEATHER_TEMP_COLOR;
    weatherForecastColor = DEFAULT_WEATHER_FORECAST_COLOR;
  }

  if (!isAnyAlarmSnoozed())
  {
    RecursiveLockGuard lock(_mutex);
    snoozeIconColor = DEFAULT_SNOOZE_ICON_COLOR;
  }

  {
    RecursiveLockGuard lock(_mutex);
    _isDirty = true;
  }
  scheduleSave();
}

/**
 * @brief Resets general settings to their default values.
 */
void ConfigManager::resetGeneralSettingsToDefaults()
{
  {
    RecursiveLockGuard lock(_mutex);
    autoBrightness = DEFAULT_AUTO_BRIGHTNESS;
    brightness = DEFAULT_BRIGHTNESS;
    autoBrightnessStartHour = DEFAULT_AUTO_BRIGHTNESS_START_HOUR;
    autoBrightnessEndHour = DEFAULT_AUTO_BRIGHTNESS_END_HOUR;
    dayBrightness = DEFAULT_DAY_BRIGHTNESS;
    nightBrightness = DEFAULT_NIGHT_BRIGHTNESS;
    use24HourFormat = DEFAULT_USE_24_HOUR_FORMAT;
    useCelsius = DEFAULT_USE_CELSIUS;
    screenFlipped = DEFAULT_SCREEN_FLIPPED;
    invertColors = DEFAULT_INVERT_COLORS;
    timezone = DEFAULT_TIMEZONE;
    isDst = DEFAULT_IS_DST;
    tempCorrectionEnabled = DEFAULT_TEMP_CORRECTION_ENABLED;
    tempCorrection = DEFAULT_TEMP_CORRECTION;

    snoozeDuration = DEFAULT_SNOOZE_DURATION;
    dismissDuration = DEFAULT_DISMISS_DURATION;
    address = DEFAULT_ADDRESS;
    enabledPages.assign(std::begin(DEFAULT_ENABLED_PAGES), std::end(DEFAULT_ENABLED_PAGES));
    defaultPage = DEFAULT_DEFAULT_PAGE;
    lat = DEFAULT_LAT;
    lon = DEFAULT_LON;
    _isDirty = true;
  }
  scheduleSave();
}

/**
 * @brief Checks if any of the configured alarms are currently snoozed.
 * @return True if at least one alarm is snoozed, false otherwise.
 */
bool ConfigManager::isAnyAlarmSnoozed() const
{
  RecursiveLockGuard lock(_mutex);
  for (const auto &alarm : _alarms)
  {
    if (alarm.isSnoozed())
    {
      return true;
    }
  }
  return false;
}

// Getters
String ConfigManager::getWifiSSID() const
{
  RecursiveLockGuard lock(_mutex);
  return wifiSSID;
}
String ConfigManager::getWifiPassword() const
{
  RecursiveLockGuard lock(_mutex);
  return wifiPassword;
}
bool ConfigManager::isAutoBrightness() const
{
  RecursiveLockGuard lock(_mutex);
  return autoBrightness;
}
uint8_t ConfigManager::getBrightness() const
{
  RecursiveLockGuard lock(_mutex);
  return brightness;
}
uint8_t ConfigManager::getAutoBrightnessStartHour() const
{
  RecursiveLockGuard lock(_mutex);
  return autoBrightnessStartHour;
}
uint8_t ConfigManager::getAutoBrightnessEndHour() const
{
  RecursiveLockGuard lock(_mutex);
  return autoBrightnessEndHour;
}
uint8_t ConfigManager::getDayBrightness() const
{
  RecursiveLockGuard lock(_mutex);
  return dayBrightness;
}
uint8_t ConfigManager::getNightBrightness() const
{
  RecursiveLockGuard lock(_mutex);
  return nightBrightness;
}
bool ConfigManager::is24HourFormat() const
{
  RecursiveLockGuard lock(_mutex);
  return use24HourFormat;
}
bool ConfigManager::isCelsius() const
{
  RecursiveLockGuard lock(_mutex);
  return useCelsius;
}
String ConfigManager::getHostname() const
{
  RecursiveLockGuard lock(_mutex);
  return hostname;
}
bool ConfigManager::isScreenFlipped() const
{
  RecursiveLockGuard lock(_mutex);
  return screenFlipped;
}
bool ConfigManager::isInvertColors() const
{
  RecursiveLockGuard lock(_mutex);
  return invertColors;
}
String ConfigManager::getTimezone() const
{
  RecursiveLockGuard lock(_mutex);
  return timezone;
}
uint8_t ConfigManager::getSnoozeDuration() const
{
  RecursiveLockGuard lock(_mutex);
  return snoozeDuration;
}
uint8_t ConfigManager::getDismissDuration() const
{
  RecursiveLockGuard lock(_mutex);
  return dismissDuration;
}
float ConfigManager::getTempCorrection() const
{
  RecursiveLockGuard lock(_mutex);
  return tempCorrection;
}
bool ConfigManager::isTempCorrectionEnabled() const
{
  RecursiveLockGuard lock(_mutex);
  return tempCorrectionEnabled;
}
bool ConfigManager::isDST() const
{
  RecursiveLockGuard lock(_mutex);
  return isDst;
}
String ConfigManager::getBackgroundColor() const
{
  RecursiveLockGuard lock(_mutex);
  return backgroundColor;
}
String ConfigManager::getTimeColor() const
{
  RecursiveLockGuard lock(_mutex);
  return timeColor;
}
String ConfigManager::getTodColor() const
{
  RecursiveLockGuard lock(_mutex);
  return todColor;
}
String ConfigManager::getSecondsColor() const
{
  RecursiveLockGuard lock(_mutex);
  return secondsColor;
}
String ConfigManager::getDayOfWeekColor() const
{
  RecursiveLockGuard lock(_mutex);
  return dayOfWeekColor;
}
String ConfigManager::getDateColor() const
{
  RecursiveLockGuard lock(_mutex);
  return dateColor;
}
String ConfigManager::getTempColor() const
{
  RecursiveLockGuard lock(_mutex);
  return tempColor;
}
String ConfigManager::getHumidityColor() const
{
  RecursiveLockGuard lock(_mutex);
  return humidityColor;
}
String ConfigManager::getAlarmIconColor() const
{
  RecursiveLockGuard lock(_mutex);
  return alarmIconColor;
}
String ConfigManager::getSnoozeIconColor() const
{
  RecursiveLockGuard lock(_mutex);
  return snoozeIconColor;
}
String ConfigManager::getAlarmTextColor() const
{
  RecursiveLockGuard lock(_mutex);
  return alarmTextColor;
}
String ConfigManager::getErrorTextColor() const
{
  RecursiveLockGuard lock(_mutex);
  return errorTextColor;
}
String ConfigManager::getWeatherTempColor() const
{
  RecursiveLockGuard lock(_mutex);
  return weatherTempColor;
}
String ConfigManager::getWeatherForecastColor() const
{
  RecursiveLockGuard lock(_mutex);
  return weatherForecastColor;
}
int8_t ConfigManager::getRingingAlarmId() const
{
  RecursiveLockGuard lock(_mutex);
  return ringingAlarmId;
}
uint32_t ConfigManager::getRingingAlarmStartTimestamp() const
{
  RecursiveLockGuard lock(_mutex);
  return ringingAlarmStartTimestamp;
}
bool ConfigManager::areWifiCredsValid() const
{
  RecursiveLockGuard lock(_mutex);
  return wifiCredsValid;
}
String ConfigManager::getAddress() const
{
  RecursiveLockGuard lock(_mutex);
  return address;
}
std::vector<int> ConfigManager::getEnabledPages() const
{
  RecursiveLockGuard lock(_mutex);
  return enabledPages;
}
int ConfigManager::getDefaultPage() const
{
  RecursiveLockGuard lock(_mutex);
  return defaultPage;
}
float ConfigManager::getLat() const
{
  RecursiveLockGuard lock(_mutex);
  return lat;
}
float ConfigManager::getLon() const
{
  RecursiveLockGuard lock(_mutex);
  return lon;
}
bool ConfigManager::isDirty() const
{
  RecursiveLockGuard lock(_mutex);
  return _isDirty;
}
void ConfigManager::clearDirtyFlag()
{
  RecursiveLockGuard lock(_mutex);
  _isDirty = false;
}

// Setters - Implemented here to properly manage locking

void ConfigManager::setWifiSSID(const String &ssid)
{
  {
    RecursiveLockGuard lock(_mutex);
    wifiSSID = ssid;
    wifiCredsValid = false;
    _isDirty = true;
  }
  scheduleSave();
}

void ConfigManager::setWifiPassword(const String &password)
{
  {
    RecursiveLockGuard lock(_mutex);
    wifiPassword = password;
    wifiCredsValid = false;
    _isDirty = true;
  }
  scheduleSave();
}

void ConfigManager::setHostname(const String &name)
{
  {
    RecursiveLockGuard lock(_mutex);
    hostname = name;
    _isDirty = true;
  }
  scheduleSave();
}

void ConfigManager::setWifiCredsValid(bool valid)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (wifiCredsValid != valid)
    {
      wifiCredsValid = valid;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setTempCorrectionEnabled(bool enabled)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (tempCorrectionEnabled != enabled)
    {
      tempCorrectionEnabled = enabled;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setTempCorrection(float value)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (tempCorrection != value)
    {
      tempCorrection = value;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setAddress(const String &addr)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (address != addr)
    {
      address = addr;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setEnabledPages(const std::vector<int> &pages)
{
  {
    RecursiveLockGuard lock(_mutex);
    enabledPages = pages;
    _isDirty = true;
  }
  scheduleSave();
}

void ConfigManager::setDefaultPage(int page)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (defaultPage != page)
    {
      defaultPage = page;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setLat(float latitude)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (lat != latitude)
    {
      lat = latitude;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setLon(float longitude)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (lon != longitude)
    {
      lon = longitude;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setDST(bool active)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (isDst != active)
    {
      isDst = active;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setInvertColors(bool inverted)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (invertColors != inverted)
    {
      invertColors = inverted;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setSnoozeDuration(uint8_t duration)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (snoozeDuration != duration)
    {
      snoozeDuration = duration;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setDismissDuration(uint8_t duration)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (dismissDuration != duration)
    {
      dismissDuration = duration;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setRingingAlarmId(int8_t id)
{
  RecursiveLockGuard lock(_mutex);
  if (ringingAlarmId != id)
  {
    ringingAlarmId = id;
  }
}

void ConfigManager::setRingingAlarmStartTimestamp(uint32_t timestamp)
{
  RecursiveLockGuard lock(_mutex);
  if (ringingAlarmStartTimestamp != timestamp)
  {
    ringingAlarmStartTimestamp = timestamp;
  }
}

void ConfigManager::setScreenFlipped(bool flipped)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (screenFlipped != flipped)
    {
      screenFlipped = flipped;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setAutoBrightness(bool enabled)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (autoBrightness != enabled)
    {
      autoBrightness = enabled;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setBrightness(uint8_t value)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (brightness != value)
    {
      brightness = value;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setAutoBrightnessStartHour(uint8_t value)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (autoBrightnessStartHour != value)
    {
      autoBrightnessStartHour = value;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setAutoBrightnessEndHour(uint8_t value)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (autoBrightnessEndHour != value)
    {
      autoBrightnessEndHour = value;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setDayBrightness(uint8_t value)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (dayBrightness != value)
    {
      dayBrightness = value;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setNightBrightness(uint8_t value)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (nightBrightness != value)
    {
      nightBrightness = value;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::set24HourFormat(bool enabled)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (use24HourFormat != enabled)
    {
      use24HourFormat = enabled;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setCelsius(bool enabled)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (useCelsius != enabled)
    {
      useCelsius = enabled;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setTimezone(const String &tz)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (timezone != tz)
    {
      timezone = tz;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setBackgroundColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (backgroundColor != color)
    {
      backgroundColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setTimeColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (timeColor != color)
    {
      timeColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setTodColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (todColor != color)
    {
      todColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setSecondsColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (secondsColor != color)
    {
      secondsColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setDayOfWeekColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (dayOfWeekColor != color)
    {
      dayOfWeekColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setDateColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (dateColor != color)
    {
      dateColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setTempColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (tempColor != color)
    {
      tempColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setHumidityColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (humidityColor != color)
    {
      humidityColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setAlarmIconColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (alarmIconColor != color)
    {
      alarmIconColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setSnoozeIconColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (snoozeIconColor != color)
    {
      snoozeIconColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setAlarmTextColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (alarmTextColor != color)
    {
      alarmTextColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setErrorTextColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (errorTextColor != color)
    {
      errorTextColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setWeatherTempColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (weatherTempColor != color)
    {
      weatherTempColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}

void ConfigManager::setWeatherForecastColor(const String &color)
{
  {
    RecursiveLockGuard lock(_mutex);
    if (weatherForecastColor != color)
    {
      weatherForecastColor = color;
      _isDirty = true;
    }
  }
  scheduleSave();
}
