// SerialLog.h
#pragma once

#include <Arduino.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include <FS.h>
#include <freertos/semphr.h>

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
   * @brief Handles periodic tasks, such as flushing the log buffer.
   */
  void loop();

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
   * @brief Enables or disables console (Serial/WebSocket) logging.
   * @param enabled True to enable, false to disable.
   */
  void setConsoleLoggingEnabled(bool enabled);

  /**
   * @brief Enables or disables file logging.
   * @param enabled True to enable, false to disable.
   */
  void setFileLoggingEnabled(bool enabled);

  /**
   * @brief Enables or disables global logging (console + file).
   * @param enabled True to enable, false to disable.
   */
  void setLoggingEnabled(bool enabled);

  /**
   * @brief Gets the log file path.
   * @return The path to the log file.
   */
  static const char *getLogFilePath() { return LOG_FILE_PATH; }

private:
  /**
   * @brief Private constructor to enforce the singleton pattern.
   */
  SerialLog();

  // The WebSocket object for logging.
  AsyncWebSocket _ws;

  // Flags to control logging outputs
  bool _consoleLoggingEnabled = true;
  bool _fileLoggingEnabled = true;

  // WebSocket event handler.
  static void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

  // Constants for file logging
  static const char *LOG_FILE_PATH;
  static const size_t MAX_LOG_SIZE;
  static const size_t BUFFER_THRESHOLD;
  static const unsigned long FLUSH_INTERVAL;

  // Buffering members
  String _logBuffer;
  unsigned long _lastFlushTime;
  SemaphoreHandle_t _mutex;

  /**
   * @brief Writes a message to the log buffer.
   * @param message The message to write.
   */
  void logToFile(const char *message);

  /**
   * @brief Flushes the log buffer to the file in LittleFS.
   */
  void flush();

  /**
   * @brief Rotates the log file when it exceeds the maximum size.
   */
  void rotateLogFile();
};
