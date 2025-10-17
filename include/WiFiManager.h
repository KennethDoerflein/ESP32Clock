// WiFiManager.h

#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <memory>
#include <WiFi.h>

// Forward declaration
class DNSServer;

/**
 * @class WiFiManager
 * @brief Manages WiFi connectivity for the ESP32.
 *
 * This singleton class handles connecting to a saved WiFi network or, if
 * unsuccessful, launching an Access Point with a captive portal for configuration.
 * It tracks the connection status and provides a central point for all WiFi operations.
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
   * network is configured, it starts a soft Access Point (AP) and a captive portal.
   * @return True if the captive portal was started, false otherwise.
   */
  bool begin();

  /**
   * @brief Handles captive portal DNS requests.
   *
   * This method should be called in the main loop to process DNS requests
   * when the captive portal is active.
   */
  void handleDns();

  /**
   * @brief Checks if the device is currently connected to a WiFi network.
   * @return True if connected, false otherwise.
   */
  bool isConnected() const;

  /**
   * @brief Checks if the captive portal is currently active.
   * @return True if the captive portal is running, false otherwise.
   */
  bool isCaptivePortal() const;

  /**
   * @brief Manages an asynchronous WiFi scan.
   *
   * Initiates a scan if not already started. Returns a JSON string with
   * the scan results or a status indicating the scan is in progress.
   * This is designed to be non-blocking.
   * @return A JSON formatted string.
   */
  String scanNetworksAsync();

  /**
   * @brief Gets the configured hostname of the device.
   * @return The hostname as a String.
   */
  String getHostname() const;

  // Delete copy constructor and assignment operator for singleton pattern.
  WiFiManager(const WiFiManager &) = delete;
  void operator=(const WiFiManager &) = delete;

private:
  /**
   * @brief Private constructor to enforce singleton pattern.
   */
  WiFiManager();

  /**
   * @brief Starts the captive portal.
   *
   * Sets up the AP and DNS server for configuration.
   */
  void startCaptivePortal();

  /**
   * @brief WiFi event handler.
   * @param event The WiFi event that occurred.
   * @param info The event data.
   */
  static void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

  /// @brief The SSID for the Access Point mode, used for initial configuration.
  static const char *AP_SSID;
  /// @brief Flag to signal a successful connection from the event handler.
  static volatile bool _connectionResult;
  /// @brief The hostname for the device, generated from the MAC address.
  String _hostname;
  /// @brief Flag indicating the current WiFi connection status.
  bool _isConnected;
  /// @brief Pointer to the DNS server instance for the captive portal.
  std::unique_ptr<DNSServer> _dnsServer;
};

#endif // WIFIMANAGER_H