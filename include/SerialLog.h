// SerialLog.h
#pragma once

#include <Arduino.h>
#include <AsyncWebSocket.h>

/**
 * @class SerialLog
 * @brief A singleton logger that mirrors Serial output to a WebSocket.
 *
 * This class provides a centralized logging facility. It can be disabled at
 * runtime and, when enabled, sends all log messages to both the hardware
 * Serial port and a WebSocket endpoint (`/ws/log`). This allows for remote
 * debugging via the web interface.
 */
class SerialLog
{
public:
  /**
   * @brief Gets the singleton instance of the SerialLog.
   * @return A reference to the singleton SerialLog instance.
   */
  static SerialLog &getInstance();

  /**
   * @brief Initializes the WebSocket logger.
   * @param server A pointer to the AsyncWebServer instance.
   */
  void begin(AsyncWebServer *server);

  /**
   * @brief Prints a message to the serial port and WebSocket.
   * @param message The message to print.
   */
  void print(const String &message);

  /**
   * @brief Prints a formatted message to the serial port and WebSocket.
   * @param format The format string (a la printf).
   * @param ... The arguments for the format string.
   */
  void printf(const char *format, ...);

  /**
   * @brief Enables or disables logging.
   * @param enabled True to enable logging, false to disable it.
   */
  void setLoggingEnabled(bool enabled);

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  SerialLog();
  AsyncWebSocket _ws;
  bool _loggingEnabled = true;
};
