// ClockWebServer.h

#ifndef CLOCK_WEBSERVER_H
#define CLOCK_WEBSERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

/**
 * @class ClockWebServer
 * @brief Manages the ESP32's web server for configuration and updates.
 *
 * This class encapsulates the setup and handling of all web-related
 * functionalities. It is implemented as a singleton.
 */
class ClockWebServer
{
public:
  static ClockWebServer &getInstance();

  void begin();

  // Delete copy constructor and assignment operator.
  ClockWebServer(const ClockWebServer &) = delete;
  void operator=(const ClockWebServer &) = delete;

private:
  // Private constructor.
  ClockWebServer();

  // The actual server instance.
  AsyncWebServer server;
};

#endif // CLOCK_WEBSERVER_H