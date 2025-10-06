// WiFiManager.h

#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>

/**
 * @class WiFiManager
 * @brief Manages WiFi connectivity for the ESP32.
 *
 * This singleton class handles connecting to a saved WiFi network or, if
 * unsuccessful, launching an Access Point for configuration. It tracks the
 * connection status and provides a central point for all WiFi operations.
 */
class WiFiManager
{
public:
  /**
   * @brief Returns the singleton instance of the WiFiManager class.
   * @return A reference to the singleton WiFiManager instance.
   */
  static WiFiManager &getInstance();

  /**
   * @brief Initializes WiFi and attempts to connect.
   *
   * Tries to connect to the configured WiFi network. If it fails or if no
   * network is configured, it starts a soft Access Point (AP).
   */
  void begin();

  /**
   * @brief Checks if the device is currently connected to a WiFi network.
   * @return True if connected, false otherwise.
   */
  bool isConnected() const;

  // Delete copy constructor and assignment operator for singleton pattern.
  WiFiManager(const WiFiManager &) = delete;
  void operator=(const WiFiManager &) = delete;

private:
  /**
   * @brief Private constructor to enforce singleton pattern.
   */
  WiFiManager();

  /// @brief The SSID for the Access Point mode, used for initial configuration.
  static const char *AP_SSID;
  /// @brief Flag indicating the current WiFi connection status.
  bool _isConnected;
};

#endif // WIFIMANAGER_H