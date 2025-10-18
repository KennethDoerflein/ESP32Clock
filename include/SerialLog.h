// SerialLog.h
#pragma once

#include <Arduino.h>
#include <AsyncWebSocket.h>

class SerialLog
{
public:
  static SerialLog &getInstance();
  void begin(AsyncWebServer *server);
  void print(const String &message);
  void printf(const char *format, ...);

private:
  SerialLog();
  AsyncWebSocket _ws;
};
