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
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  ClockWebServer();

  // --- Request Handlers ---

  /**
   * @brief Handles requests to the root URL ("/").
   * @param request The incoming web request.
   */
  void onRootRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles requests for the WiFi configuration page.
   * @param request The incoming web request.
   */
  void onWifiRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles the form submission for saving WiFi credentials.
   * @param request The incoming web request.
   */
  void onWifiSaveRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles API requests to test WiFi credentials.
   * @param request The incoming web request.
   */
  void onWifiTestRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles API requests for the current WiFi connection status.
   * @param request The incoming web request.
   */
  void onWifiStatusRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles POST requests to save general settings.
   * @param request The incoming web request.
   */
  void onSettingsRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles POST requests to save display settings.
   * @param request The incoming web request.
   */
  void onDisplayRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles POST requests to save alarm settings.
   * @param request The incoming web request.
   */
  void onAlarmsRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles all requests when in captive portal mode.
   * @param request The incoming web request.
   */
  void onCaptivePortalRequest(AsyncWebServerRequest *request);

  /**
   * @brief Handles captive portal redirection for iOS/Android.
   * @param request The incoming web request.
   */
  void onCaptivePortalRedirect(AsyncWebServerRequest *request);

  // --- Template Processor and Helpers ---

  /**
   * @brief A template processor for replacing placeholders in HTML pages.
   * @param var The placeholder variable name.
   * @return The value to replace the placeholder with.
   */
  String processor(const String &var);

  /**
   * @brief A specialized template processor for the settings page.
   * @param var The placeholder variable name.
   * @return The value to replace the placeholder with.
   */
  String settingsProcessor(const String &var);

  /// The actual server instance.
  AsyncWebServer server;
  /// Flag indicating if the server is in captive portal mode.
  bool _captivePortalActive;
};

#endif // CLOCK_WEBSERVER_H