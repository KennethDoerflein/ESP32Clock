/**
 * @file SerialLog.cpp
 * @brief Implements the SerialLog class for logging to Serial and WebSocket.
 *
 * This file contains the implementation of the singleton logger, which provides
 * a centralized way to handle log messages.
 */
#include "SerialLog.h"

/**
 * @brief Constructs a new SerialLog instance.
 * Initializes the WebSocket on the "/ws" endpoint.
 */
SerialLog::SerialLog() : _ws("/ws/log") {}

/**
 * @brief Gets the singleton instance of the SerialLog.
 * @return A reference to the singleton SerialLog instance.
 */
SerialLog &SerialLog::getInstance()
{
  static SerialLog instance;
  return instance;
}

/**
 * @brief Initializes the WebSocket logger and attaches it to the web server.
 * @param server A pointer to the AsyncWebServer instance.
 */
void SerialLog::begin(AsyncWebServer *server)
{
  server->addHandler(&_ws);
}

/**
 * @brief Enables or disables logging.
 *
 * When disabled, calls to `print` and `printf` will be ignored.
 *
 * @param enabled True to enable logging, false to disable.
 */
void SerialLog::setLoggingEnabled(bool enabled)
{
  _loggingEnabled = enabled;
}

/**
 * @brief Prints a message to the Serial port and sends it to all WebSocket clients.
 * @param message The message to be logged.
 */
void SerialLog::print(const String &message)
{
  if (!_loggingEnabled)
    return;
  Serial.println(message);
  _ws.textAll(message);
}

/**
 * @brief Prints a formatted message to the Serial port and sends it to all WebSocket clients.
 * @param format The format string (a la printf).
 * @param ... The arguments for the format string.
 */
void SerialLog::printf(const char *format, ...)
{
  if (!_loggingEnabled)
    return;
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.println(buf);
  _ws.textAll(buf);
}
