// SerialLog.cpp
#include "SerialLog.h"

SerialLog::SerialLog() : _ws("/ws") {}

SerialLog &SerialLog::getInstance()
{
  static SerialLog instance;
  return instance;
}

void SerialLog::begin(AsyncWebServer *server)
{
  server->addHandler(&_ws);
}

void SerialLog::print(const String &message)
{
  Serial.print(message);
  _ws.textAll(message);
}

void SerialLog::printf(const char *format, ...)
{
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.print(buf);
  _ws.textAll(buf);
}
