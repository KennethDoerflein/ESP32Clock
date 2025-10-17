// WiFiManager.cpp

#include "WiFiManager.h"
#include <WiFi.h>
#include <DNSServer.h>
#include "ConfigManager.h"
#include "display.h"
#include "ClockWebServer.h"
#include <ArduinoJson.h>

// --- Static Member Initialization ---
const char *WiFiManager::AP_SSID = "ESP32-Clock-Setup";
volatile bool WiFiManager::_connectionResult = false;

void WiFiManager::wifiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
  {
    Serial.println("\nWiFi connected! Got IP.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
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
    Serial.println("WiFi lost connection.");
    _connectionResult = false;
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
  // Set a unique hostname based on the last 3 bytes of the MAC address.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char hostname_cstr[18]; // "ESP32Clock_" + 6 hex chars + null terminator
  sprintf(hostname_cstr, "ESP32Clock_%02X%02X%02X", mac[3], mac[4], mac[5]);
  _hostname = hostname_cstr;
  WiFi.setHostname(_hostname.c_str());
  Serial.printf("Hostname set to: %s\n", _hostname.c_str());

  // Register the event handler
  WiFi.onEvent(wifiEventHandler);

  // --- WiFi Connection Logic ---
  String ssid = ConfigManager::getInstance().getWifiSSID();
  String password = ConfigManager::getInstance().getWifiPassword();
  auto &display = Display::getInstance();

  if (ssid.length() > 0)
  {
    Serial.printf("WiFiManager: Attempting to connect to SSID: %s\n", ssid.c_str());
    display.drawStatusMessage("Connecting to WiFi...");
    _connectionResult = false; // Reset the flag
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (!_connectionResult && millis() - startTime < 15000)
    { // 15-second timeout
      delay(100);
      Serial.print(".");
    }

    if (_connectionResult)
    {
      Serial.println("\nWiFiManager: Connection successful.");
      _isConnected = true;
      display.drawStatusMessage(("IP: " + WiFi.localIP().toString()).c_str());
      delay(2000);
    }
    else
    {
      Serial.println("\nWiFiManager: Connection failed after 15 seconds.");
    }
  }
  else
  {
    Serial.println("WiFiManager: No SSID configured.");
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
  Serial.println("\nStarting Captive Portal.");
  display.drawStatusMessage(("Setup AP: " + String(AP_SSID)).c_str());

  // Start AP
  WiFi.softAP(AP_SSID);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);

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