/**
 * @file SerialLog.cpp
 * @brief Implements the SerialLog class for logging to Serial and WebSocket.
 *
 * This file contains the implementation of the singleton logger, which provides
 * a centralized way to handle log messages.
 */
#include "SerialLog.h"

// Initialize static members
const char *SerialLog::LOG_FILE_PATH = "/system.log";
const size_t SerialLog::MAX_LOG_SIZE = 20 * 1024;     // 20KB
const size_t SerialLog::BUFFER_THRESHOLD = 256;       // 256 Bytes
const unsigned long SerialLog::FLUSH_INTERVAL = 2000; // 2 Seconds

/**
 * @brief Constructs a new SerialLog instance.
 * Initializes the WebSocket on the "/ws" endpoint.
 */
SerialLog::SerialLog() : _ws("/ws/log"), _lastFlushTime(0)
{
  _mutex = xSemaphoreCreateMutex();
  _logBuffer.reserve(BUFFER_THRESHOLD + 64); // Pre-allocate to reduce fragmentation
}

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
 * @brief Handles periodic tasks, such as flushing the log buffer.
 */
void SerialLog::loop()
{
  if (xSemaphoreTake(_mutex, 0) == pdTRUE)
  { // Non-blocking check
    if (_logBuffer.length() > 0 && (millis() - _lastFlushTime >= FLUSH_INTERVAL))
    {
      flush();
    }
    xSemaphoreGive(_mutex);
  }
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
 * @brief Enables or disables console (Serial/WebSocket) logging.
 * @param enabled True to enable, false to disable.
 */
void SerialLog::setConsoleLoggingEnabled(bool enabled)
{
  _consoleLoggingEnabled = enabled;
}

/**
 * @brief Enables or disables file logging.
 * @param enabled True to enable, false to disable.
 */
void SerialLog::setFileLoggingEnabled(bool enabled)
{
  _fileLoggingEnabled = enabled;
}

/**
 * @brief Enables or disables global logging.
 *
 * @param enabled True to enable logging (both), false to disable both.
 */
void SerialLog::setLoggingEnabled(bool enabled)
{
  _consoleLoggingEnabled = enabled;
  _fileLoggingEnabled = enabled;
}

/**
 * @brief Prints a message to the Serial port and sends it to all WebSocket clients.
 * @param message The message to be logged.
 */
void SerialLog::print(const String &message)
{
  if (_consoleLoggingEnabled)
  {
    Serial.println(message);
    _ws.textAll(message);
  }
  if (_fileLoggingEnabled)
  {
    logToFile(message.c_str());
  }
}

/**
 * @brief Prints a formatted message to the Serial port and sends it to all WebSocket clients.
 * @param format The format string (a la printf).
 * @param ... The arguments for the format string.
 */
void SerialLog::printf(const char *format, ...)
{
  // Avoid doing vsnprintf if neither log target is enabled
  if (!_consoleLoggingEnabled && !_fileLoggingEnabled)
    return;

  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  if (_consoleLoggingEnabled)
  {
    Serial.println(buf);
    _ws.textAll(buf);
  }
  if (_fileLoggingEnabled)
  {
    logToFile(buf);
  }
}

/**
 * @brief Writes a message to the log buffer.
 * @param message The message to write.
 */
void SerialLog::logToFile(const char *message)
{
  if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
  {
    _logBuffer += message;
    _logBuffer += '\n';

    if (_logBuffer.length() >= BUFFER_THRESHOLD)
    {
      flush();
    }
    xSemaphoreGive(_mutex);
  }
}

/**
 * @brief Flushes the log buffer to the file in LittleFS.
 * Assumes the mutex is already held by the caller.
 */
void SerialLog::flush()
{
  if (_logBuffer.length() == 0)
    return;

  File logFile = LittleFS.open(LOG_FILE_PATH, "a");
  if (logFile)
  {
    if (logFile.size() >= MAX_LOG_SIZE)
    {
      logFile.close();
      rotateLogFile(); // This assumes rotating doesn't need the mutex or is fast
      logFile = LittleFS.open(LOG_FILE_PATH, "a");
    }

    if (logFile)
    {
      logFile.print(_logBuffer);
      logFile.close();
      _logBuffer = "";
      _lastFlushTime = millis();
    }
  }
  else
  {
    // If file open fails, we might want to clear the buffer to prevent indefinite growth
    // or keep it to try again?
    // For safety against OOM, if buffer gets too huge, clear it.
    if (_logBuffer.length() > 2048)
    {
      _logBuffer = "";
    }
  }
}

/**
 * @brief Rotates the log file when it exceeds the maximum size.
 */
void SerialLog::rotateLogFile()
{
  String oldLogPath = String(LOG_FILE_PATH) + ".old";

  // Remove the old backup if it exists
  if (LittleFS.exists(oldLogPath))
  {
    LittleFS.remove(oldLogPath);
  }

  // Rename current log to .old
  if (LittleFS.exists(LOG_FILE_PATH))
  {
    LittleFS.rename(LOG_FILE_PATH, oldLogPath);
  }
}
