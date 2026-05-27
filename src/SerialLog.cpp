// SerialLog.cpp

/**
 * @file SerialLog.cpp
 * @brief Implements the SerialLog class for logging to Serial and WebSocket.
 *
 * This file contains the implementation of the singleton logger, which provides
 * a centralized way to handle log messages.
 */
#include "SerialLog.h"
#include "UpdateManager.h"
#include "LockGuard.h"
#include <esp_attr.h>
#include <esp_system.h>
#include <time.h>
#include <freertos/task.h>

// RTC Memory for Crash Logging
#define CRASH_LOG_MAGIC 0xDEADBEEF
#define CRASH_LOG_SIZE 1024

struct RtcCrashLog
{
  uint32_t magic;
  size_t head;
  bool wrapped;
  char buffer[CRASH_LOG_SIZE];
};

RTC_NOINIT_ATTR RtcCrashLog g_crashLog;

// Initialize static members
const char *SerialLog::LOG_FILE_PATH = "/system.log";
const size_t SerialLog::MAX_LOG_SIZE = 256 * 1024;    // 256KB
const char *SerialLog::CRASH_FILE_PATH = "/crash.log";
const size_t SerialLog::MAX_CRASH_LOG_SIZE = 64 * 1024; // 64KB
const size_t SerialLog::BUFFER_THRESHOLD = 256;       // 256 Bytes
const unsigned long SerialLog::FLUSH_INTERVAL = 2000; // 2 Seconds

/**
 * @brief Constructs a new SerialLog instance.
 * Initializes the WebSocket on the "/ws" endpoint.
 */
SerialLog::SerialLog() : _ws("/ws/log"), _lastFlushTime(0)
{
  _mutex = xSemaphoreCreateRecursiveMutex();
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
  if (xSemaphoreTakeRecursive(_mutex, 0) == pdTRUE)
  { // Non-blocking check
    if (_logBuffer.length() > 0 && (millis() - _lastFlushTime >= FLUSH_INTERVAL))
    {
      flush();
    }
    xSemaphoreGiveRecursive(_mutex);
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
  RecursiveLockGuard lock(_mutex);
  _consoleLoggingEnabled = enabled;
}

/**
 * @brief Enables or disables file logging.
 * @param enabled True to enable, false to disable.
 */
void SerialLog::setFileLoggingEnabled(bool enabled)
{
  RecursiveLockGuard lock(_mutex);
  _fileLoggingEnabled = enabled;
}

/**
 * @brief Enables or disables global logging.
 *
 * @param enabled True to enable logging (both), false to disable both.
 */
void SerialLog::setLoggingEnabled(bool enabled)
{
  RecursiveLockGuard lock(_mutex);
  _consoleLoggingEnabled = enabled;
  _fileLoggingEnabled = enabled;
}

/**
 * @brief Prints a message to the Serial port and sends it to all WebSocket clients.
 * @param message The message to be logged.
 */
// Returns a timestamp prefix string, e.g. "[2026-03-03 21:02:54] " or "[+12345ms] ".
// Uses POSIX time() so it reflects NTP/RTC-synced time automatically.
static String getTimestamp()
{
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  // Retrieve current task name safely
  const char *taskName = "unknown";
  TaskHandle_t curTask = xTaskGetCurrentTaskHandle();
  if (curTask != nullptr)
  {
    taskName = pcTaskGetName(curTask);
  }

  char buf[64];
  // If year is before 2021 the clock hasn't been synced yet — show uptime.
  if (t.tm_year + 1900 < 2021)
  {
    snprintf(buf, sizeof(buf), "[+%lums] [%s] ", (unsigned long)millis(), taskName);
  }
  else
  {
    snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec, taskName);
  }
  return String(buf);
}

void SerialLog::print(const String &message)
{
  RecursiveLockGuard lock(_mutex);

  String ts = getTimestamp();
  String prefixed = ts + message;

  if (_consoleLoggingEnabled)
  {
    if (prefixed.endsWith("\n"))
    {
      Serial.print(prefixed);
      if (_ws.count() > 0) _ws.textAll(prefixed);
    }
    else
    {
      Serial.println(prefixed);
      if (_ws.count() > 0) _ws.textAll(prefixed + "\n");
    }
  }
  if (_fileLoggingEnabled)
  {
    logToFile(prefixed.c_str());
  }
}

/**
 * @brief Prints a formatted message to the Serial port and sends it to all WebSocket clients.
 * @param format The format string (a la printf).
 * @param ... The arguments for the format string.
 */
void SerialLog::printf(const char *format, ...)
{
  RecursiveLockGuard lock(_mutex);

  // Avoid doing vsnprintf if neither log target is enabled
  if (!_consoleLoggingEnabled && !_fileLoggingEnabled)
    return;

  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  String ts = getTimestamp();
  String prefixed = ts + buf;

  if (_consoleLoggingEnabled)
  {
    if (prefixed.endsWith("\n"))
    {
      Serial.print(prefixed);
      if (_ws.count() > 0) _ws.textAll(prefixed);
    }
    else
    {
      Serial.println(prefixed);
      if (_ws.count() > 0) _ws.textAll(prefixed + "\n");
    }
  }
  if (_fileLoggingEnabled)
  {
    logToFile(prefixed.c_str());
  }
}

/**
 * @brief Writes a message to the log buffer.
 * @param message The message to write.
 */
void SerialLog::logToFile(const char *message)
{
  if (UpdateManager::getInstance().isUpdateInProgress())
    return;

  // Append to RTC Crash Log
  // We do this here to capture the formatted message.
  if (g_crashLog.magic == CRASH_LOG_MAGIC)
  {
    if (g_crashLog.head >= CRASH_LOG_SIZE)
    {
      g_crashLog.head = 0;
      g_crashLog.wrapped = false;
    }
    size_t len = strlen(message);
    for (size_t i = 0; i < len; i++)
    {
      g_crashLog.buffer[g_crashLog.head] = message[i];
      g_crashLog.head = (g_crashLog.head + 1) % CRASH_LOG_SIZE;
      if (g_crashLog.head == 0)
        g_crashLog.wrapped = true;
    }
    // Ensure newline in RTC log too
    if (len == 0 || message[len - 1] != '\n')
    {
      if (g_crashLog.head >= CRASH_LOG_SIZE)
      {
        g_crashLog.head = 0;
        g_crashLog.wrapped = false;
      }
      g_crashLog.buffer[g_crashLog.head] = '\n';
      g_crashLog.head = (g_crashLog.head + 1) % CRASH_LOG_SIZE;
      if (g_crashLog.head == 0)
        g_crashLog.wrapped = true;
    }
  }

  // Mutex is already held by print/printf/loop
  _logBuffer += message;
  size_t len = strlen(message);
  if (len == 0 || message[len - 1] != '\n')
  {
    _logBuffer += '\n';
  }

  if (_logBuffer.length() >= BUFFER_THRESHOLD)
  {
    flush();
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
    // If file open fails, clear the buffer early to prevent OOM.
    // 1024 bytes is enough to hold recent context without risking heap exhaustion.
    if (_logBuffer.length() > 1024)
    {
      _logBuffer = "";
    }
  }
}

/**
 * @brief Manually rotates the log file.
 * Thread-safe.
 */
void SerialLog::rotate()
{
  RecursiveLockGuard lock(_mutex);
  rotateLogFile();
}

/**
 * @brief Rotates the log file when it exceeds the maximum size.
 */
void SerialLog::rotateLogFile()
{
  // Mutex should be held by caller
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

  // Immediately create a new empty log file to minimize the window where it doesn't exist
  File logFile = LittleFS.open(LOG_FILE_PATH, "w");
  if (logFile)
  {
    logFile.close();
  }
}

/**
 * @brief Logs the reason for the last reset.
 */
void SerialLog::logResetReason()
{
  esp_reset_reason_t reason = esp_reset_reason();
  String reasonStr;

  switch (reason)
  {
  case ESP_RST_UNKNOWN:
    reasonStr = "Unknown Reset";
    break;
  case ESP_RST_POWERON:
    reasonStr = "Power On Reset";
    break;
  case ESP_RST_EXT:
    reasonStr = "External Reset";
    break;
  case ESP_RST_SW:
    reasonStr = "Software Reset";
    break;
  case ESP_RST_PANIC:
    reasonStr = "Exception/Panic Reset";
    break;
  case ESP_RST_INT_WDT:
    reasonStr = "Interrupt Watchdog Reset";
    break;
  case ESP_RST_TASK_WDT:
    reasonStr = "Task Watchdog Reset";
    break;
  case ESP_RST_WDT:
    reasonStr = "Other Watchdog Reset";
    break;
  case ESP_RST_DEEPSLEEP:
    reasonStr = "Deep Sleep Reset";
    break;
  case ESP_RST_BROWNOUT:
    reasonStr = "Brownout Reset";
    break;
  case ESP_RST_SDIO:
    reasonStr = "SDIO Reset";
    break;
  default:
    reasonStr = "Unknown (" + String((int)reason) + ")";
    break;
  }

  // Save the magic and temporarily disable crash logging to prevent circular self-overwriting/corruption.
  uint32_t oldMagic = g_crashLog.magic;
  g_crashLog.magic = 0;

  print("RESET REASON: " + reasonStr + "\n");

  // Check for crash log in RTC memory
  if (oldMagic == CRASH_LOG_MAGIC)
  {
    // Sanity check head bounds
    if (g_crashLog.head >= CRASH_LOG_SIZE)
    {
      g_crashLog.head = 0;
      g_crashLog.wrapped = false;
    }

    if (g_crashLog.head > 0 || g_crashLog.wrapped)
    {
      // Open the crash log file in LittleFS for appending the crash details
      File crashFile = LittleFS.open(CRASH_FILE_PATH, "a");
      if (crashFile)
      {
        String bootTime = getTimestamp();
        crashFile.printf("\n=========================================\n");
        crashFile.printf("CRASH DETECTED ON BOOT: %s", bootTime.c_str());
        crashFile.printf("RESET REASON: %s\n", reasonStr.c_str());
        crashFile.printf("-----------------------------------------\n");
      }

      print("--- CRASH DUMP FROM PREVIOUS SESSION ---\n");

      String dump = "";
      dump.reserve(512);
      size_t start = g_crashLog.wrapped ? g_crashLog.head : 0;
      size_t count = g_crashLog.wrapped ? CRASH_LOG_SIZE : g_crashLog.head;

      for (size_t i = 0; i < count; i++)
      {
        char c = g_crashLog.buffer[(start + i) % CRASH_LOG_SIZE];
        if (c == '\0') continue; // Skip nulls
        dump += c;
        if (dump.length() >= 512)
        {
          print(dump);
          if (crashFile)
          {
            crashFile.print(dump);
          }
          dump = "";
        }
      }
      if (dump.length() > 0)
      {
        print(dump);
        if (crashFile)
        {
          crashFile.print(dump);
        }
      }

      print("\n--- END CRASH DUMP ---\n");

      if (crashFile)
      {
        crashFile.printf("\n=========================================\n");
        crashFile.close();
        
        // Check size and rotate if necessary
        rotateCrashLogFile();
      }
    }
    else
    {
      print("Crash log buffer was empty.\n");
    }
  }
  else
  {
    print("Crash log magic signature not found (Cold Boot?).\n");
  }

  // Initialize/Reset RTC Log for the new session
  g_crashLog.magic = CRASH_LOG_MAGIC;
  g_crashLog.head = 0;
  g_crashLog.wrapped = false;
  memset(g_crashLog.buffer, 0, CRASH_LOG_SIZE);
  
  // Log some initial session info
  printf("New session started. Max heap: %u, Free heap: %u\n", ESP.getHeapSize(), ESP.getFreeHeap());
}

/**
 * @brief Rotates the crash log file when it exceeds the maximum size.
 */
void SerialLog::rotateCrashLogFile()
{
  // Check size of the crash log file
  File crashFile = LittleFS.open(CRASH_FILE_PATH, "r");
  if (!crashFile)
    return;

  size_t size = crashFile.size();
  crashFile.close();

  if (size < MAX_CRASH_LOG_SIZE)
    return;

  // Rotation needed
  String oldCrashPath = String(CRASH_FILE_PATH) + ".old";

  // Remove the old backup if it exists
  if (LittleFS.exists(oldCrashPath))
  {
    LittleFS.remove(oldCrashPath);
  }

  // Rename current crash log to .old
  LittleFS.rename(CRASH_FILE_PATH, oldCrashPath);

  // Immediately create a new empty crash log file
  File newFile = LittleFS.open(CRASH_FILE_PATH, "w");
  if (newFile)
  {
    newFile.close();
  }
}

void SerialLog::cleanupClients()
{
  RecursiveLockGuard lock(_mutex);
  _ws.cleanupClients();
}

// --- Diagnostic Crash Hooks ---
// IMPORTANT: These handlers execute in crash/fault context. They must NEVER
// take a mutex (deadlock risk) or call any function that allocates heap memory.
// We write directly to the RTC buffer and Serial, then restart.

/**
 * @brief Helper to append a message to the RTC crash buffer.
 * Safe to call from any context (ISR, crash handler, etc.)
 * because it only touches RTC_NOINIT memory with raw pointer arithmetic.
 */
static void IRAM_ATTR writeToCrashBuffer(const char *msg)
{
  if (g_crashLog.magic != CRASH_LOG_MAGIC)
    return;
  if (g_crashLog.head >= CRASH_LOG_SIZE)
  {
    g_crashLog.head = 0;
    g_crashLog.wrapped = false;
  }
  size_t len = strlen(msg);
  for (size_t i = 0; i < len; i++)
  {
    g_crashLog.buffer[g_crashLog.head] = msg[i];
    g_crashLog.head = (g_crashLog.head + 1) % CRASH_LOG_SIZE;
    if (g_crashLog.head == 0)
      g_crashLog.wrapped = true;
  }
}

/**
 * @brief Shutdown handler registered with esp_register_shutdown_handler().
 * Fires on any panic, abort(), assert failure, or ESP.restart().
 * Stamps the RTC crash buffer so we know the system was shutting down.
 * The ESP-IDF's built-in panic handler already prints assertion details,
 * backtraces, and register dumps to Serial before this runs.
 */
static void onShutdown()
{
  writeToCrashBuffer("\n[SHUTDOWN HANDLER] System shutting down.\n");
}

/**
 * @brief Registers the shutdown handler. Must be called once during setup().
 */
void SerialLog::registerCrashHandlers()
{
  esp_register_shutdown_handler(onShutdown);
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  // This hook may be called from ISR context on ESP32.
  // Avoid ALL FreeRTOS API, heap allocations, and mutex operations.
  char buf[128];
  snprintf(buf, sizeof(buf), "\n[STACK OVERFLOW] Task: '%s' overflowed its stack!\n", pcTaskName ? pcTaskName : "?");

  writeToCrashBuffer(buf);

  // ets_printf is ROM-resident and safe to call from ISR/crash context
  ets_printf("%s", buf);
  esp_restart();
}

extern "C" void vApplicationMallocFailedHook(void)
{
  const char *buf = "\n[MALLOC FAILED] Out of Memory!\n";

  writeToCrashBuffer(buf);

  Serial.print(buf);
  Serial.flush();
  delay(500);
  esp_restart();
}

