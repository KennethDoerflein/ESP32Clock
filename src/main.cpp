// main.cpp

/**
 * @file main.cpp
 * @brief The main entry point for the ESP32 Clock application.
 *
 * This file contains the primary setup and loop functions that orchestrate
 * the device's operation, including initialization of all modules, handling
 * WiFi connectivity, and managing the main application logic loop.
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WebServer.h"
#include <WiFi.h>

// Include all custom module headers.
#include "ConfigManager.h"
#include "sensors.h"
#include "display.h"
#include "TimeManager.h"
#include "ntp.h"

// --- Global Constants ---
/// @brief The interval for reading sensor data, in milliseconds.
const unsigned long SENSOR_UPDATE_INTERVAL = 10000; // 10 seconds
/// @brief The SSID for the Access Point mode, used for initial configuration.
const char *AP_SSID = "ESP32-Clock-Setup";

// --- Global Variables ---
/// @brief Stores the timestamp of the last sensor update for interval timing.
unsigned long prevSensorMillis = 0;
/// @brief Global instance of the web server.
WebServer webServer;
/// @brief Flag indicating the current WiFi connection status.
bool isWifiConnected = false;

/**
 * @brief The main setup function, run once on boot.
 *
 * Initializes serial communication, managers, sensors, display, and WiFi.
 * It attempts to connect to a configured WiFi network. If it fails or if
 * no network is configured, it starts an Access Point for web-based setup.
 */
void setup()
{
  Serial.begin(115200);

  // Initialize the configuration manager and load settings from LittleFS.
  ConfigManager::getInstance().begin();
  // Initialize hardware sensors (RTC, BME280).
  setupSensors();

  // Initialize the display.
  auto &display = Display::getInstance();
  display.begin();
  display.drawStatusMessage("Initializing...");

  // Set a unique hostname based on the last 3 bytes of the MAC address.
  // This ensures that each device has a unique identifier on the network.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char hostname[18]; // "ESP32Clock_" + 6 hex chars + null terminator
  sprintf(hostname, "ESP32Clock_%02X%02X%02X", mac[3], mac[4], mac[5]);
  WiFi.setHostname(hostname);
  Serial.printf("Hostname set to: %s\n", hostname);

  // --- WiFi Connection Logic ---
  String ssid = ConfigManager::getInstance().getWifiSSID();
  String password = ConfigManager::getInstance().getWifiPassword();

  // Attempt to connect only if an SSID has been configured.
  if (ssid.length() > 0)
  {
    display.drawStatusMessage("Connecting to WiFi...");
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection with a timeout to prevent getting stuck.
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000)
    { // 15-second timeout
      delay(500);
      Serial.print(".");
    }

    // Check if the connection was successful.
    if (WiFi.status() == WL_CONNECTED)
    {
      isWifiConnected = true;
      Serial.println("\nWiFi connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      display.drawStatusMessage(("IP: " + WiFi.localIP().toString()).c_str());
      delay(2000); // Display the IP address for a moment.
    }
  }

  // If WiFi is not connected (either failed or not configured), start AP mode.
  if (!isWifiConnected)
  {
    Serial.println("\nStarting AP mode.");
    WiFi.softAP(AP_SSID);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(apIP);
    display.drawStatusMessage(("Setup AP: " + String(AP_SSID)).c_str());
  }

  // Start the web server in all cases (AP mode or connected mode).
  webServer.begin();

  // If connected, proceed with time synchronization and main display setup.
  if (isWifiConnected)
  {
    display.drawStatusMessage("Syncing Time...");
    TimeManager::getInstance().begin(); // This will trigger the first NTP sync.
    display.drawLayout();               // Clear the screen for the main clock UI.
  }
  else
  {
    // If not connected, display a message prompting the user to configure WiFi.
    display.drawStatusMessage("Connect to AP & set WiFi");
  }
}

/**
 * @brief The main application loop.
 *
 * This function runs continuously. If WiFi is connected, it updates the
 * time, brightness, and display elements. It also periodically reads and
 * displays sensor data. If WiFi is not connected, it does nothing.
 */
void loop()
{
  delay(10); // Small delay to yield to other tasks.

  // Only run the main clock logic if WiFi is connected.
  if (isWifiConnected)
  {
    unsigned long now = millis();
    auto &timeManager = TimeManager::getInstance();
    auto &display = Display::getInstance();

    // Perform periodic tasks for the time manager (e.g., daily sync check).
    timeManager.update();

    // Adjust screen brightness based on current settings (manual or auto).
    display.updateBrightness();

    // Redraw the main display components with the latest time and date.
    display.drawClock(timeManager.getFormattedTime().c_str(), timeManager.getTOD().c_str());
    display.drawDate(timeManager.getFormattedDate().c_str());
    display.drawDayOfWeek(timeManager.getDayOfWeek().c_str());

    // --- Sensor Update Logic (runs on a separate interval) ---
    if (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL)
    {
      prevSensorMillis = now; // Reset the timer.
      bool useCelsius = ConfigManager::getInstance().isCelsius();
      float temp = readTemperature(useCelsius);
      float hum = readHumidity();
      display.drawTemperature(temp, useCelsius);
      display.drawHumidity(hum);
    }
  }
}