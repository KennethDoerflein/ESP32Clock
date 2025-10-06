// utils.h

#pragma once
#include <Arduino.h>

/**
 * @file utils.h
 * @brief Provides miscellaneous utility functions and constants.
 *
 * This file declares globally accessible constants and helper functions
 * that don't belong to a specific class but are used across the application.
 */

/// @brief An array of three-letter abbreviations for the days of the week, starting with Sunday.
extern const char *dayNames[];
/// @brief An array of three-letter abbreviations for the months of the year.
extern const char *monthNames[];

/**
 * @brief Prints detailed information about the ESP32 chip to the Serial monitor.
 *
 * This function is useful for debugging and diagnostics, providing information
 * such as chip model, revision, number of cores, and flash size.
 */
void printChipInfo();