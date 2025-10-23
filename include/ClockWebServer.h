// ClockWebServer.h

#ifndef CLOCK_WEBSERVER_H
#define CLOCK_WEBSERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

/**
 * @class ClockWebServer
 * @brief Manages the ESP32's web server for configuration and updates.
 *
 * This class encapsulates the setup and handling of all web-related
 * functionalities. It is implemented as a singleton. It can operate in a
 * normal mode or a captive portal mode for initial WiFi setup.
 */
class ClockWebServer
{
public:
  /**
   * @brief Returns the singleton instance of the ClockWebServer.
   */
  static ClockWebServer &getInstance();

  /**
   * @brief Starts the web server.
   *
   * Sets up routes based on whether captive portal mode is enabled.
   */
  void begin();

  /**
   * @brief Enables captive portal mode.
   *
   * This method should be called before `begin()`. It configures the server
   * to respond to all requests with the WiFi setup page.
   */
  void enableCaptivePortal();

  /**
   * @brief Sets up the mDNS responder.
   *
   * Initializes and starts the mDNS service to advertise the web server.
   * This should be called after a successful WiFi connection.
   */
  void setupMDNS();

  // Delete copy constructor and assignment operator.
  ClockWebServer(const ClockWebServer &) = delete;
  void operator=(const ClockWebServer &) = delete;

private:
  // Private constructor.
  ClockWebServer();

  // --- Request Handlers ---
  void onRootRequest(AsyncWebServerRequest *request);
  void onWifiRequest(AsyncWebServerRequest *request);
  void onWifiSaveRequest(AsyncWebServerRequest *request);
  void onWifiTestRequest(AsyncWebServerRequest *request);
  void onWifiStatusRequest(AsyncWebServerRequest *request);
  void onSettingsRequest(AsyncWebServerRequest *request);
  void onDisplayRequest(AsyncWebServerRequest *request);
  void onAlarmsRequest(AsyncWebServerRequest *request);
  void onCaptivePortalRequest(AsyncWebServerRequest *request);
  void onCaptivePortalRedirect(AsyncWebServerRequest *request);

  // --- Template Processor and Helpers ---
  String processor(const String &var);
  String settingsProcessor(const String &var);

  /// The actual server instance.
  AsyncWebServer server;
  /// Flag indicating if the server is in captive portal mode.
  bool _captivePortalActive;
};

#endif // CLOCK_WEBSERVER_H