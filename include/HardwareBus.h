// HardwareBus.h
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @class I2CBus
 * @brief Standalone synchronization wrapper providing a recursive mutex
 * for serializing physical I2C hardware bus access across all peripherals
 * (DS3231 RTC, BME280 sensor, etc.) on multi-core ESP32.
 *
 * Decouples hardware I2C peripheral locking from TimeManager's internal
 * state mutex to eliminate cross-core ABBA deadlocks.
 */
class I2CBus
{
public:
  /**
   * @brief Gets the shared recursive mutex guarding the physical I2C bus.
   * @return FreeRTOS recursive mutex handle.
   */
  static SemaphoreHandle_t getMutex()
  {
    static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    return mutex;
  }
};
