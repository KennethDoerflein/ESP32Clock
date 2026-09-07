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
#include "TimeManager.h"
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
static portMUX_TYPE g_crashLogMux = portMUX_INITIALIZER_UNLOCKED;

// Initialize static members
const char *SerialLog::LOG_FILE_PATH = "/system.log";
const size_t SerialLog::MAX_LOG_SIZE = 256 * 1024;    // 256KB
const char *SerialLog::CRASH_FILE_PATH = "/crash.log";
const size_t SerialLog::MAX_CRASH_LOG_SIZE = 64 * 1024; // 64KB
const size_t SerialLog::BUFFER_THRESHOLD = 512;       // 512 Bytes
const unsigned long SerialLog::FLUSH_INTERVAL = 5000; // 5 Seconds

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
  if (xSemaphoreTakeRecursive(_mutex, pdMS_TO_TICKS(50)) == pdTRUE)
  {
    if (_logBuffer.length() > 0 && ((millis() - _lastFlushTime >= FLUSH_INTERVAL) || _logBuffer.length() >= BUFFER_THRESHOLD))
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
static void getTimestamp(char* buf, size_t maxLen)
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

  // If year is before 2021 the clock hasn't been synced yet — show uptime.
  if (t.tm_year + 1900 < 2021)
  {
    snprintf(buf, maxLen, "[+%lums] [%s] ", (unsigned long)millis(), taskName);
  }
  else
  {
    snprintf(buf, maxLen, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec, taskName);
  }
}

void SerialLog::print(const String &message)
{
  char tsBuf[64];
  getTimestamp(tsBuf, sizeof(tsBuf));

  RecursiveLockGuard lock(_mutex);
  if (!_consoleLoggingEnabled && !_fileLoggingEnabled)
    return;
  
  bool needsNewline = !message.endsWith("\n");
  
  // Use reserve to avoid multiple allocations when concatenating.
  String prefixed;
  prefixed.reserve(strlen(tsBuf) + message.length() + (needsNewline ? 1 : 0));
  prefixed += tsBuf;
  prefixed += message;
  if (needsNewline) {
    prefixed += '\n';
  }

  if (_consoleLoggingEnabled)
  {
    Serial.print(prefixed);
    if (_ws.count() > 0) _ws.textAll(prefixed);
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
  char tsBuf[64];
  getTimestamp(tsBuf, sizeof(tsBuf));

  RecursiveLockGuard lock(_mutex);

  // Avoid doing vsnprintf if neither log target is enabled
  if (!_consoleLoggingEnabled && !_fileLoggingEnabled)
    return;

  char msgBuf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(msgBuf, sizeof(msgBuf), format, args);
  va_end(args);
  
  size_t msgLen = strlen(msgBuf);
  bool needsNewline = (msgLen > 0 && msgBuf[msgLen - 1] != '\n');
  
  String prefixed;
  prefixed.reserve(strlen(tsBuf) + msgLen + (needsNewline ? 1 : 0));
  prefixed += tsBuf;
  prefixed += msgBuf;
  if (needsNewline) {
    prefixed += '\n';
  }

  if (_consoleLoggingEnabled)
  {
    Serial.print(prefixed);
    if (_ws.count() > 0) _ws.textAll(prefixed);
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
    // Compute length outside the critical section to keep it as short as possible.
    // portENTER_CRITICAL on ESP32-S3 disables interrupts and spin-locks the other
    // core; we must not do any non-trivial work (like strlen over a long string)
    // while holding it.
    size_t len = strlen(message);
    portENTER_CRITICAL(&g_crashLogMux);
    if (g_crashLog.head >= CRASH_LOG_SIZE)
    {
      g_crashLog.head = 0;
      g_crashLog.wrapped = false;
    }
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
    portEXIT_CRITICAL(&g_crashLogMux);
  }

  // Mutex is already held by print/printf/loop
  _logBuffer += message;
  size_t len = strlen(message);
  if (len == 0 || message[len - 1] != '\n')
  {
    _logBuffer += '\n';
  }

  // Cap the in-memory buffer to prevent unbounded heap growth if flush is delayed.
  // NEVER perform LittleFS file I/O from arbitrary calling tasks (e.g. WeatherUpdate, loopTask)
  // because concurrent LittleFS operations across tasks/cores cause LittleFS internal corruption
  // and assertion crashes (lfs_file_close). All LittleFS disk writes must only occur in loop() on LogicTask.
  static const size_t MAX_LOG_BUFFER_CAP = 4096;
  if (_logBuffer.length() > MAX_LOG_BUFFER_CAP)
  {
    // Discard oldest bytes to keep within cap
    _logBuffer = _logBuffer.substring(_logBuffer.length() - (MAX_LOG_BUFFER_CAP / 2));
  }
}

/**
 * @brief Initializes the tracked file size of the active log file.
 * Assumes the mutex is already held by the caller.
 */
void SerialLog::initLogSize()
{
  _logSizeInitialized = true;
  _currentLogSize = 0;
  File f = LittleFS.open(LOG_FILE_PATH, "r");
  if (f)
  {
    _currentLogSize = f.size();
    f.close();
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

  if (UpdateManager::getInstance().isUpdateInProgress())
    return;

  if (!_logSizeInitialized)
  {
    initLogSize();
  }

  // If appending the current buffer will exceed MAX_LOG_SIZE (or if file is already at max),
  // rotate first so we start a clean new log file without opening the full file in append mode.
  if (_currentLogSize + _logBuffer.length() >= MAX_LOG_SIZE)
  {
    rotateLogFile();
  }

  // Open in "r+" (read/write without truncation) and seek to end ourselves, rather than
  // using "a" mode. fopen("a") internally calls fseek(SEEK_END) as a separate VFS operation
  // after open(), and in esp_littlefs this second call can assert (lfs_mlist_isopen) if the
  // file handle was not fully registered in lfs->mlist — particularly after a rename().
  // Using "r+"→seek-to-end bypasses newlib's append-mode fseek entirely.
  // If the file doesn't exist yet (after rotation or first boot), use "w" to create it.
  File logFile;
  if (LittleFS.exists(LOG_FILE_PATH))
  {
    logFile = LittleFS.open(LOG_FILE_PATH, "r+");
    if (logFile)
    {
      if (!logFile.seek(0, SeekEnd))
      {
        // seek() failed — the lfs_file handle is in a bad state (e.g. opened
        // immediately after a rename() before LittleFS committed superblock).
        // Reset the File so the close() below is skipped and we avoid the
        // lfs_file_close assert on a null lfs_file pointer.
        logFile.close();
        logFile = File();
      }
    }
  }
  else
  {
    logFile = LittleFS.open(LOG_FILE_PATH, "w");
  }

  if (logFile)
  {
    size_t written = logFile.print(_logBuffer);
    logFile.close();
    _currentLogSize += written;
    _logBuffer = "";
    _lastFlushTime = millis();
  }
  else
  {
    // File open failed (or seek failed above) — reinitialize size tracking on
    // next flush so we re-read actual size rather than relying on a stale estimate.
    _logSizeInitialized = false;

    // Clear the buffer to prevent OOM if writes keep failing.
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

  // Directly remove old backup and rename current log without calling exists(),
  // which causes unnecessary open/close cycles in LittleFS.
  LittleFS.remove(oldLogPath);
  bool renamed = LittleFS.rename(LOG_FILE_PATH, oldLogPath);

  if (renamed)
  {
    _currentLogSize = 0;
    // Yield briefly after rename so LittleFS can fully commit directory metadata
    // before the next open(). Without this, lfs_file_open can race the superblock
    // commit and produce a null lfs_file_t, triggering the lfs_mlist_isopen assert.
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  else
  {
    // Rename failed — force size re-read on next flush so we don't permanently
    // mistrack the file size and loop on rotation attempts.
    _logSizeInitialized = false;
  }
}

/**
 * @brief Logs the reason for the last reset.
 */
void SerialLog::logResetReason()
{
  RecursiveLockGuard lock(_mutex);
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
      bool rotateNeeded = false;
      {
        // Open the crash log file in LittleFS for appending the crash details.
        // Do NOT use "a" mode: fopen("a") in esp_littlefs performs a second
        // fseek(SEEK_END) VFS call after open, which can hit lfs_mlist_isopen
        // assert if the lfs_file handle is null (same bug fixed in flush()).
        // Use "r+"→seek-to-end when the file already exists, or "w" to create it.
        File crashFile;
        if (LittleFS.exists(CRASH_FILE_PATH))
        {
          crashFile = LittleFS.open(CRASH_FILE_PATH, "r+");
          if (crashFile)
          {
            if (!crashFile.seek(0, SeekEnd))
            {
              // Seek failed — handle is in a bad state; close and fall through
              // without writing so we don't trigger lfs_file_close assert.
              crashFile.close();
              crashFile = File();
            }
          }
        }
        else
        {
          crashFile = LittleFS.open(CRASH_FILE_PATH, "w");
        }

        if (crashFile)
        {
          char bootTime[64];
          getTimestamp(bootTime, sizeof(bootTime));
          crashFile.printf("\n=========================================\n");
          crashFile.printf("CRASH DETECTED ON BOOT: %s", bootTime);
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
          size_t size = crashFile.size();
          
          // Check size and rotate if necessary
          if (size >= MAX_CRASH_LOG_SIZE)
          {
            rotateNeeded = true;
          }
          crashFile.close();
        }
      }
      
      if (rotateNeeded)
      {
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
  // Rotation needed
  String oldCrashPath = String(CRASH_FILE_PATH) + ".old";

  // Directly remove old crash log and rename current without exists() overhead
  LittleFS.remove(oldCrashPath);
  LittleFS.rename(CRASH_FILE_PATH, oldCrashPath);
}

void SerialLog::cleanupClients()
{
  RecursiveLockGuard lock(_mutex);
  _ws.cleanupClients();
}

void SerialLog::lock()
{
  xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
}

void SerialLog::unlock()
{
  xSemaphoreGiveRecursive(_mutex);
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
  portENTER_CRITICAL_ISR(&g_crashLogMux);
  if (g_crashLog.magic != CRASH_LOG_MAGIC) {
    portEXIT_CRITICAL_ISR(&g_crashLogMux);
    return;
  }
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
  portEXIT_CRITICAL_ISR(&g_crashLogMux);
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
  // Use a static buffer — the task stack has overflowed, so a stack-allocated
  // local of any meaningful size risks clobbering adjacent memory before restart.
  static char buf[128];
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

  ets_printf("%s", buf);
  esp_restart();
}

void SerialLog::clearCrashLogMagic()
{
  g_crashLog.magic = 0;
}

