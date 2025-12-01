// WiFiManager.h

#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <memory>
#include <WiFi.h>
#include <freertos/semphr.h>

/**
 * @class WiFiManager
 * @brief Manages WiFi connections, captive portal, and connection testing.
 *
 * This singleton class handles the entire lifecycle of the WiFi connection,
 * including initial connection, reconnection logic, captive portal for
 * configuration, and non-blocking connection tests.
 */
class WiFiManager
{
public:
  enum ConnectionTestStatus
  {
    TEST_IDLE,
    TEST_IN_PROGRESS,
    TEST_SUCCESS,
    TEST_FAILED
  };

  /**
   * @brief Gets the singleton instance of the WiFiManager.
   * @return A reference to the singleton WiFiManager instance.
   */
  static WiFiManager &getInstance();

  /**
   * @brief Initializes the WiFi module and attempts to connect.
   *
   * If credentials are saved, it tries to connect. If not, or if connection
   * fails and credentials are invalid, it starts the captive portal.
   *
   * @return True if the captive portal was started, false otherwise.
   */
  bool begin();

  /**
   * @brief Manages the WiFi connection state.
   *
   * Handles automatic reconnection logic and updates connection status.
   * Should be called in the main loop.
   */
  void handleConnection();

  /**
   * @brief Handles DNS requests for the captive portal.
   */
  void handleDns();

  /**
   * @brief Checks if the device is connected to WiFi.
   * @return True if connected, false otherwise.
   */
  bool isConnected() const;

  /**
   * @brief Checks if the captive portal is currently active.
   * @return True if the captive portal is active, false otherwise.
   */
  bool isCaptivePortal() const;

  /**
   * @brief Starts a non-blocking WiFi scan.
   */
  void startScan();

  /**
   * @brief Gets the results of the WiFi scan as a JSON string.
   * @return A JSON string containing scan results or status.
   */
  String getScanResults();

  /**
   * @brief Gets the configured hostname.
   * @return The hostname.
   */
  String getHostname() const;

  /**
   * @brief Sets the hostname.
   * @param hostname The new hostname.
   */
  void setHostname(const String &hostname);

  /**
   * @brief Starts a non-blocking connection test with new credentials.
   * @param ssid The SSID to test.
   * @param password The password to test.
   * @param saveOnSuccess If true, save credentials on success.
   */
  void startConnectionTest(const String &ssid, const String &password, bool saveOnSuccess);

  /**
   * @brief Saves credentials and reboots the device.
   * @param ssid The SSID to save.
   * @param password The password to save.
   */
  void saveCredentialsAndReboot(const String &ssid, const String &password);

  /**
   * @brief Gets the status of the current connection test.
   * @return The connection test status.
   */
  ConnectionTestStatus getConnectionTestStatus() const;

  /**
   * @brief Resets the connection test status to IDLE.
   */
  void resetConnectionTestStatus();

  /**
   * @brief Checks if a reboot is pending after a successful test.
   * @return True if a reboot is pending.
   */
  bool isPendingReboot() const;

private:
  WiFiManager();
  // Disable copy constructor and assignment operator
  WiFiManager(const WiFiManager &) = delete;
  WiFiManager &operator=(const WiFiManager &) = delete;

  static const char *AP_SSID;

  bool _isConnected;
  std::unique_ptr<DNSServer> _dnsServer;
  String _hostname;

  unsigned long _lastReconnectAttempt = 0;
  bool _isReconnecting = false;

  // Connection Test Variables
  volatile ConnectionTestStatus _testStatus = TEST_IDLE;
  String _testSsid;
  String _testPassword;
  bool _saveOnSuccess = false;
  bool _pendingReboot = false;
  volatile bool _ignoreDisconnectEvent = false;

  // Static event handler for WiFi events
  static void wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info);

  // Helper to start the captive portal
  void startCaptivePortal();

  // Flag to communicate connection result from event handler
  static volatile bool _connectionResult;

  // Mutex for thread safety
  mutable SemaphoreHandle_t _mutex;
};
