// WiFiManager.cpp

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
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
  {
    logger.print("\nWiFi connected! Got IP.\n");
    logger.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    // Set the flag to indicate a successful connection.
    _connectionResult = true;
    // Since we have an IP, it's time to start mDNS.
    // We need to ensure the web server has been initialized first,
    if (WiFi.isConnected())
    {
      ClockWebServer::getInstance().setupMDNS();
    }
  }
  else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
  {
    logger.print("WiFi lost connection.\n");
    _connectionResult = false;
    // Set _isConnected to false directly, but don't trigger reconnection here.
    // The handle() method will manage the reconnection process.
    getInstance()._isConnected = false;
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
WiFiManager::WiFiManager() : _isConnected(false), _dnsServer(nullptr) {}

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
    startCaptivePortal();
    return true; // Captive portal was started
  }

  return false; // WiFi connected successfully
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

String WiFiManager::scanNetworksAsync()
{
  int16_t scanResult = WiFi.scanComplete();

  if (scanResult == WIFI_SCAN_FAILED)
  {
    // Scan failed, so start a new one
    WiFi.scanNetworks(true); // true = async
    return "{\"status\":\"scanning\"}";
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

    // After retrieving the results, clear them and start a new scan
    // This makes the "Scan" button on the frontend responsive.
    WiFi.scanDelete();
    WiFi.scanNetworks(true);

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
  display.drawStatusMessage(("Setup AP: " + String(AP_SSID)).c_str());

  // Start AP
  WiFi.softAP(AP_SSID);
  IPAddress apIP = WiFi.softAPIP();
  logger.printf("AP IP address: %s\n", apIP.toString().c_str());

  // Start DNS Server
  _dnsServer.reset(new DNSServer());
  _dnsServer->start(53, "*", apIP);
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