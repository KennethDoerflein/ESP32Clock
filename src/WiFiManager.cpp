// WiFiManager.cpp

#include "WiFiManager.h"
#include <WiFi.h>
#include <DNSServer.h>
#include "ConfigManager.h"
#include "display.h"

// --- Static Member Initialization ---
const char *WiFiManager::AP_SSID = "ESP32-Clock-Setup";

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
  char hostname[18]; // "ESP32Clock_" + 6 hex chars + null terminator
  sprintf(hostname, "ESP32Clock_%02X%02X%02X", mac[3], mac[4], mac[5]);
  WiFi.setHostname(hostname);
  Serial.printf("Hostname set to: %s\n", hostname);

  // --- WiFi Connection Logic ---
  String ssid = ConfigManager::getInstance().getWifiSSID();
  String password = ConfigManager::getInstance().getWifiPassword();
  auto &display = Display::getInstance();

  if (ssid.length() > 0)
  {
    display.drawStatusMessage("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000)
    { // 15-second timeout
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      _isConnected = true;
      Serial.println("\nWiFi connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      display.drawStatusMessage(("IP: " + WiFi.localIP().toString()).c_str());
      delay(2000);
    }
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