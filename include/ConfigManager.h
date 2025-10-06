// ConfigManager.h

#pragma once

#include <Arduino.h>

/**
 * @class ConfigManager
 * @brief Manages the application's configuration settings using a singleton pattern.
 *
 * This class is responsible for loading, saving, and providing access to
 * configuration settings such as WiFi credentials, display preferences, and
 * other system options. Settings are stored in a JSON file on the LittleFS filesystem.
 */
class ConfigManager
{
public:
  /**
   * @brief Gets the singleton instance of the ConfigManager.
   * @return A reference to the singleton ConfigManager instance.
   */
  static ConfigManager &getInstance()
  {
    static ConfigManager instance;
    return instance;
  }

  /**
   * @brief Initializes the ConfigManager.
   *
   * This method mounts the LittleFS filesystem and loads the configuration
   * from the JSON file. If the file doesn't exist, it creates one with
   * default values. This should be called once at startup.
   */
  void begin();

  /**
   * @brief Saves the current configuration to the JSON file on LittleFS.
   * @return True if the configuration was saved successfully, false otherwise.
   */
  bool save();

  // Getters
  /**
   * @brief Gets the stored WiFi SSID.
   * @return The WiFi SSID as a String.
   */
  String getWifiSSID() const { return wifiSSID; }

  /**
   * @brief Gets the stored WiFi password.
   * @return The WiFi password as a String.
   */
  String getWifiPassword() const { return wifiPassword; }

  /**
   * @brief Checks if automatic brightness control is enabled.
   * @return True if auto brightness is enabled, false otherwise.
   */
  bool isAutoBrightness() const { return autoBrightness; }

  /**
   * @brief Gets the manual brightness level.
   * @return The brightness level (0-255).
   */
  uint8_t getBrightness() const { return brightness; }

  /**
   * @brief Checks if the clock is in 24-hour format.
   * @return True if 24-hour format is enabled, false otherwise.
   */
  bool is24HourFormat() const { return use24HourFormat; }

  /**
   * @brief Checks if the temperature is displayed in Celsius.
   * @return True if Celsius is used, false for Fahrenheit.
   */
  bool isCelsius() const { return useCelsius; }

  // Setters
  /**
   * @brief Sets the WiFi SSID.
   * @param ssid The new WiFi SSID.
   */
  void setWifiSSID(const String &ssid) { wifiSSID = ssid; }

  /**
   * @brief Sets the WiFi password.
   * @param password The new WiFi password.
   */
  void setWifiPassword(const String &password) { wifiPassword = password; }

  /**
   * @brief Enables or disables automatic brightness control.
   * @param enabled True to enable, false to disable.
   */
  void setAutoBrightness(bool enabled) { autoBrightness = enabled; }

  /**
   * @brief Sets the manual brightness level.
   * @param value The brightness level (0-255).
   */
  void setBrightness(uint8_t value) { brightness = value; }

  /**
   * @brief Sets the clock to 24-hour or 12-hour format.
   * @param enabled True for 24-hour, false for 12-hour.
   */
  void set24HourFormat(bool enabled) { use24HourFormat = enabled; }

  /**
   * @brief Sets the temperature unit.
   * @param enabled True for Celsius, false for Fahrenheit.
   */
  void setCelsius(bool enabled) { useCelsius = enabled; }

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  ConfigManager() {}

  // Configuration variables with default values
  String wifiSSID = "";
  String wifiPassword = "";
  bool autoBrightness = true;
  uint8_t brightness = 255;
  bool use24HourFormat = false;
  bool useCelsius = false;

  /**
   * @brief Loads the configuration from the JSON file.
   *
   * If the file cannot be read or parsed, default values are used.
   */
  void load();

  /**
   * @brief Sets the configuration variables to their default values.
   */
  void setDefaults();
};