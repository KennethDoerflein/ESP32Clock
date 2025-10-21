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
   * @brief Handles WiFi connection and reconnection logic.
   *
   * This method should be called periodically in the main loop to manage
   * the WiFi connection. It will attempt to reconnect if the connection
   * is lost.
   */
  void handleConnection();

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
  void startScan();

  /**
   * @brief Manages an asynchronous WiFi scan.
   *
   * Initiates a scan if not already started. Returns a JSON string with
   * the scan results or a status indicating the scan is in progress.
   * This is designed to be non-blocking.
   * @return A JSON formatted string.
   */
  String getScanResults();

  /**
   * @brief Gets the configured hostname of the device.
   * @return The hostname as a String.
   */
  String getHostname() const;

  /**
   * @brief Sets the hostname of the device.
   * @param hostname The new hostname.
   */
  void setHostname(const String &hostname);

  /**
   * @brief The status of a WiFi connection test.
   */
  enum ConnectionTestStatus
  {
    TEST_IDLE,
    TEST_IN_PROGRESS,
    TEST_SUCCESS,
    TEST_FAILED
  };

  /**
   * @brief Starts a non-blocking test of WiFi credentials.
   * @param ssid The SSID to test.
   * @param password The password to test.
   * @param saveOnSuccess If true, credentials will be saved on success.
   */
  void startConnectionTest(const String &ssid, const String &password, bool saveOnSuccess);

  /**
   * @brief Gets the current status of the connection test.
   * @return The ConnectionTestStatus.
   */
  ConnectionTestStatus getConnectionTestStatus() const;

  /**
   * @brief Resets the connection test status to idle.
   */
  void resetConnectionTestStatus();

  /**
   * @brief Checks if a reboot is pending after a successful WiFi save.
   * @return True if a reboot is pending, false otherwise.
   */
  bool isPendingReboot() const;

  /**
   * @brief Saves new WiFi credentials and triggers a reboot. For use outside of captive portal.
   * @param ssid The SSID to save.
   * @param password The password to save.
   */
  void saveCredentialsAndReboot(const String &ssid, const String &password);

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

  /**
   * @brief Flag to tell the event handler to ignore the next disconnect event.
   * This is used to prevent the intentional disconnect at the start of a test
   * from being misinterpreted as a connection failure.
   */
  volatile bool _ignoreDisconnectEvent = false;

  /**
   * @brief Flag to indicate that an intentional disconnection is in progress for a connection test.
   * Used by the event handler's state machine to know when to start a new connection attempt.
   */
  volatile bool _disconnectingForTest = false;
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
  /// @brief Timestamp of the last reconnection attempt.
  unsigned long _lastReconnectAttempt = 0;
  /// @brief Flag to indicate if a reconnection is in progress.
  bool _isReconnecting = false;
  /// @brief The current status of the connection test.
  volatile ConnectionTestStatus _testStatus = TEST_IDLE;
  /// @brief Flag to trigger a reboot after the client has been notified of success.
  volatile bool _pendingReboot = false;
  /// @brief If true, credentials will be saved on successful test.
  bool _saveOnSuccess = false;
  /// @brief The SSID being tested.
  String _testSsid;
  /// @brief The password being tested.
  String _testPassword;
};

#endif // WIFIMANAGER_H