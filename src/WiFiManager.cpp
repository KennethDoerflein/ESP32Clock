// src/WiFiManager.cpp

#include "WiFiManager.h"
#include <WiFi.h>
#include <DNSServer.h>
#include "ConfigManager.h"
#include "display.h"
#include "ClockWebServer.h"
#include <ArduinoJson.h>
#include "SerialLog.h"

// --- Static Member Initialization ---
const char *WiFiManager::AP_SSID = "Clock-Setup";
volatile bool WiFiManager::_connectionResult = false;

void WiFiManager::wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info)
{
  auto &logger = SerialLog::getInstance();
  auto &instance = getInstance();

  // --- Handle Connection Test Events ---
  if (instance._testStatus == TEST_IN_PROGRESS)
  {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
      // Check if this is the intentional disconnect from the start of the test.
      if (instance._ignoreDisconnectEvent)
      {
        // If it is, consume the flag and do nothing.
        // We now wait for either a successful connection or a real failure.
        instance._ignoreDisconnectEvent = false;
      }
      else
      {
        // If the flag is already false, this is a genuine failure.
        logger.print("\nConnection test failed.\n");
        instance._testStatus = TEST_FAILED;
        if (instance.isCaptivePortal())
        {
          WiFi.mode(WIFI_AP);
        }
      }
    }
    else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
    {
      logger.print("\nConnection test successful!\n");
      instance._testStatus = TEST_SUCCESS;
      instance._ignoreDisconnectEvent = false; // Reset flag on success, just in case.

      if (instance._saveOnSuccess)
      {
        auto &config = ConfigManager::getInstance();
        config.setWifiSSID(instance._testSsid);
        config.setWifiPassword(instance._testPassword);
        config.save();
        instance._pendingReboot = true;
      }
    }
    return; // Do not fall through to standard handling during a test.
  }

  // --- Standard Event Handling (for when not testing) ---
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
  {
    logger.print("\nWiFi connected! Got IP.\n");
    logger.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    // Set the flag to indicate a successful connection.
    _connectionResult = true;

    // If this is the first successful connection with these credentials,
    // mark them as valid and save the config.
    auto &config = ConfigManager::getInstance();
    if (!config.areWifiCredsValid())
    {
      logger.print("WiFi credentials validated. Saving flag.\n");
      config.setWifiCredsValid(true);
      config.save();
    }

    // Since we have an IP, it's time to start mDNS.
    // We need to ensure the web server has been initialized first,
    if (WiFi.isConnected())
    {
      WiFi.mode(WIFI_STA);
      logger.print("Switched to STA mode. AP is now off.\n");
      ClockWebServer::getInstance().setupMDNS();
    }
  }
  else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
  {
    logger.print("WiFi lost connection.\n");
    _connectionResult = false;
    // Set _isConnected to false directly, but don't trigger reconnection here.
    instance._isConnected = false;
  }
}

void WiFiManager::handleConnection()
{
  if (isCaptivePortal())
  {
    return;
  }

  if (_isConnected)
  {
    if (_isReconnecting)
    {
      _isReconnecting = false;
    }
    return;
  }

  if (ConfigManager::getInstance().getWifiSSID().length() == 0)
  {
    return;
  }

  unsigned long now = millis();

  if (_isReconnecting)
  {
    if (now - _lastReconnectAttempt > 15000) // 15s timeout
    {
      SerialLog::getInstance().print("WiFi reconnection timed out.\n");
      _isReconnecting = false;
    }
    return;
  }

  if (now - _lastReconnectAttempt > 30000) // 30s interval
  {
    _lastReconnectAttempt = now;
    _isReconnecting = true;
    SerialLog::getInstance().print("Attempting to reconnect WiFi...\n");
    WiFi.reconnect();
  }
}

// --- Singleton Implementation ---
WiFiManager &WiFiManager::getInstance()
{
  static WiFiManager instance;
  return instance;
}

/**
 * @brief Private constructor for the WiFiManager.
 * Initializes the connection status to false.
 */
WiFiManager::WiFiManager() : _isConnected(false), _dnsServer(nullptr), _pendingReboot(false) {}

// --- Public Methods ---
/**
 * @brief Initializes the WiFi module.
 *
 * Sets a unique hostname, then attempts to connect to the WiFi network using
 * credentials stored in the ConfigManager. If the SSID is not configured or
 * the connection fails, it launches a captive portal for configuration.
 * It also provides visual feedback on the display during this process.
 * @return True if the captive portal was started, false otherwise.
 */
bool WiFiManager::begin()
{
  _hostname = ConfigManager::getInstance().getHostname();

  if (_hostname.length() == 0)
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char hostname_cstr[18]; // "ESP32Clock_" + 6 hex chars + null terminator
    sprintf(hostname_cstr, "ESP32Clock_%02X%02X%02X", mac[3], mac[4], mac[5]);
    _hostname = hostname_cstr;
  }

  WiFi.setHostname(_hostname.c_str());
  SerialLog::getInstance().printf("Hostname set to: %s\n", _hostname.c_str());

  // Register the event handler
  WiFi.onEvent(wifiEventHandler);
  WiFi.setSleep(false);

  // --- WiFi Connection Logic ---
  String ssid = ConfigManager::getInstance().getWifiSSID();
  String password = ConfigManager::getInstance().getWifiPassword();
  auto &display = Display::getInstance();
  auto &logger = SerialLog::getInstance();

  if (ssid.length() > 0)
  {
    logger.printf("WiFiManager: Attempting to connect to SSID: %s\n", ssid.c_str());
    display.drawStatusMessage("Connecting to WiFi...");
    _connectionResult = false; // Reset the flag
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (!_connectionResult && millis() - startTime < 15000)
    { // 15-second timeout
      delay(100);
      logger.print(".");
    }

    if (_connectionResult)
    {
      logger.print("\nWiFiManager: Connection successful.\n");
      _isConnected = true;
      display.drawStatusMessage(("IP: " + WiFi.localIP().toString()).c_str());
      delay(2000);
    }
    else
    {
      logger.print("\nWiFiManager: Connection failed after 15 seconds.\n");
    }
  }
  else
  {
    logger.print("WiFiManager: No SSID configured.\n");
  }

  if (!_isConnected)
  {
    // Disconnect and stop the station to prevent it from retrying
    // with bad credentials in the background, which causes log spam.
    WiFi.disconnect(true);

    // Only start captive portal if credentials have not been validated before.
    if (!ConfigManager::getInstance().areWifiCredsValid())
    {
      logger.print("WiFi credentials are not validated. Starting Captive Portal.\n");
      startCaptivePortal();
      return true; // Captive portal was started
    }
    else
    {
      logger.print("WiFi connection failed, but credentials are valid. Skipping captive portal.\n");
    }
  }

  return false; // WiFi connected successfully, or skipping captive portal for background retry.
}

/**
 * @brief Handles DNS requests when the captive portal is active.
 * This should be called repeatedly in the main loop.
 */
void WiFiManager::handleDns()
{
  if (_dnsServer)
  {
    _dnsServer->processNextRequest();
  }
}

/**
 * @brief Returns the current WiFi connection status.
 * @return True if the device is connected to WiFi, false otherwise.
 */
bool WiFiManager::isConnected() const
{
  return _isConnected;
}

/**
 * @brief Checks if the captive portal is currently active.
 * @return True if the captive portal is running, false otherwise.
 */
bool WiFiManager::isCaptivePortal() const
{
  return _dnsServer != nullptr;
}

void WiFiManager::startScan()
{
  // Start a non-blocking scan.
  // The results will be retrieved asynchronously by getScanResults().
  WiFi.scanNetworks(true);
}

String WiFiManager::getScanResults()
{
  int16_t scanResult = WiFi.scanComplete();

  if (scanResult == WIFI_SCAN_FAILED)
  {
    // Scan failed, but we don't start a new one here.
    // We just report that the scan is not running.
    return "{\"status\":\"idle\"}";
  }
  else if (scanResult == WIFI_SCAN_RUNNING)
  {
    // Scan is in progress
    return "{\"status\":\"scanning\"}";
  }
  else
  {
    // Scan is complete, and we have results
    JsonDocument doc;
    JsonArray networks = doc.to<JsonArray>();

    for (int i = 0; i < scanResult; ++i)
    {
      JsonObject network = networks.add<JsonObject>();
      network["ssid"] = WiFi.SSID(i);
      network["rssi"] = WiFi.RSSI(i);
      network["channel"] = WiFi.channel(i);
      // Use a string for encryption type for easier parsing on the frontend
      switch (WiFi.encryptionType(i))
      {
      case WIFI_AUTH_OPEN:
        network["encryption"] = "OPEN";
        break;
      case WIFI_AUTH_WEP:
        network["encryption"] = "WEP";
        break;
      case WIFI_AUTH_WPA_PSK:
        network["encryption"] = "WPA_PSK";
        break;
      case WIFI_AUTH_WPA2_PSK:
        network["encryption"] = "WPA2_PSK";
        break;
      case WIFI_AUTH_WPA_WPA2_PSK:
        network["encryption"] = "WPA_WPA2_PSK";
        break;
      case WIFI_AUTH_WPA2_ENTERPRISE:
        network["encryption"] = "WPA2_ENTERPRISE";
        break;
      case WIFI_AUTH_WPA3_PSK:
        network["encryption"] = "WPA3_PSK";
        break;
      case WIFI_AUTH_WPA2_WPA3_PSK:
        network["encryption"] = "WPA2_WPA3_PSK";
        break;
      default:
        network["encryption"] = "UNKNOWN";
        break;
      }
    }

    // After retrieving the results, clear them.
    WiFi.scanDelete();

    String jsonOutput;
    serializeJson(doc, jsonOutput);
    return jsonOutput;
  }
}

// --- Private Methods ---

/**
 * @brief Starts the Access Point and DNS server for the captive portal.
 */
void WiFiManager::startCaptivePortal()
{
  auto &display = Display::getInstance();
  auto &logger = SerialLog::getInstance();
  logger.print("\nStarting Captive Portal.\n");

  // Set mode to AP + Station
  WiFi.mode(WIFI_AP_STA);

  // Start AP
  WiFi.softAP(AP_SSID);
  IPAddress apIP = WiFi.softAPIP();
  logger.printf("AP IP address: %s\n", apIP.toString().c_str());

  // Start DNS Server
  _dnsServer.reset(new DNSServer());
  const byte DNS_PORT = 53;

  _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  _dnsServer->start(DNS_PORT, "*", apIP);

  // Show a message on the display while scanning
  logger.print("Starting background WiFi scan...\n");
  display.drawMultiLineStatusMessage("Please wait...", "Scanning for networks");
  WiFi.scanNetworks(true); // true = async (non-blocking)
  delay(5000);
}

/**
 * @brief Gets the configured hostname of the device.
 * @return The hostname as a String.
 */
String WiFiManager::getHostname() const
{
  return _hostname;
}

void WiFiManager::setHostname(const String &hostname)
{
  _hostname = hostname;
  ConfigManager::getInstance().setHostname(hostname);
  ConfigManager::getInstance().save();
  // Update mDNS
  if (WiFi.isConnected())
  {
    ClockWebServer::getInstance().setupMDNS();
  }
}

void WiFiManager::startConnectionTest(const String &ssid, const String &password, bool saveOnSuccess)
{
  if (_pendingReboot)
  {
    SerialLog::getInstance().print("Ignoring new connection test, reboot is pending.\n");
    return;
  }

  auto &logger = SerialLog::getInstance();
  logger.printf("Starting connection test for SSID: %s\n", ssid.c_str());

  _testSsid = ssid;
  _testPassword = password;
  _saveOnSuccess = saveOnSuccess;
  _testStatus = TEST_IN_PROGRESS;

  // Set a flag to tell the event handler to ignore the next disconnect event,
  // which we are about to cause intentionally.
  _ignoreDisconnectEvent = true;

  WiFi.disconnect(true);
  delay(100);

  if (isCaptivePortal())
  {
    WiFi.mode(WIFI_AP_STA);
  }

  WiFi.begin(ssid.c_str(), password.c_str());
}

void WiFiManager::saveCredentialsAndReboot(const String &ssid, const String &password)
{
  auto &config = ConfigManager::getInstance();
  config.setWifiSSID(ssid);
  config.setWifiPassword(password);
  config.save();

  // A reboot is the most reliable way to apply new WiFi credentials
  ESP.restart();
}

WiFiManager::ConnectionTestStatus WiFiManager::getConnectionTestStatus() const
{
  return _testStatus;
}

bool WiFiManager::isPendingReboot() const
{
  return _pendingReboot;
}

void WiFiManager::resetConnectionTestStatus()
{
  _testStatus = TEST_IDLE;
  _pendingReboot = false;
}