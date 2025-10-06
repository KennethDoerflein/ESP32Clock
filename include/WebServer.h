// WebServer.h

#ifndef WEBSERVER_H
#define WEBSERVER_H

/**
 * @class WebServer
 * @brief Manages the ESP32's web server for configuration and updates.
 *
 * This class encapsulates the setup and handling of all web-related
 * functionalities, including serving pages for WiFi setup, system settings,
 * and firmware updates (both manual and from GitHub).
 */
class WebServer
{
public:
  /**
   * @brief Initializes and starts the asynchronous web server.
   *
   * This method sets up all the URL routes (endpoints) for the web interface
   * and starts the server. It should be called once during the application setup.
   */
  void begin();
};

#endif // WEBSERVER_H