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
  _ws.onEvent(onEvent);
  server->addHandler(&_ws);
}

/**
 * @brief Handles WebSocket events.
 * @param server The WebSocket server.
 * @param client The WebSocket client.
 * @param type The type of event.
 * @param arg A pointer to additional arguments.
 * @param data A pointer to the data.
 * @param len The length of the data.
 */
void SerialLog::onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    // client connected
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // client disconnected
    Serial.printf("ws[%s][%u] disconnect\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    // error was received from the other side
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    // pong message was received (in response to a ping)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
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
