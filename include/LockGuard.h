#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class LockGuard
{
public:
  LockGuard(SemaphoreHandle_t mutex) : _mutex(mutex)
  {
    if (_mutex)
      xSemaphoreTake(_mutex, portMAX_DELAY);
  }
  ~LockGuard()
  {
    if (_mutex)
      xSemaphoreGive(_mutex);
  }

private:
  SemaphoreHandle_t _mutex;
};

class RecursiveLockGuard
{
public:
  RecursiveLockGuard(SemaphoreHandle_t mutex) : _mutex(mutex)
  {
    if (_mutex)
      xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
  }
  ~RecursiveLockGuard()
  {
    if (_mutex)
      xSemaphoreGiveRecursive(_mutex);
  }

private:
  SemaphoreHandle_t _mutex;
};
