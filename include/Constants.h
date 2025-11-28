/**
 * @file Constants.h
 * @brief Global constants and pin definitions for the ESP32 Clock.
 *
 * This file centralizes hardware pin assignments, timing values, and other
 * fixed parameters used throughout the application.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

// --- Pin Definitions ---
#define SNOOZE_BUTTON_PIN 5 ///< GPIO pin for the snooze/navigation button.
#define BOOT_BUTTON_PIN 0   ///< GPIO pin for the built-in boot button (used for factory reset).
#define RTC_INT_PIN 2       ///< GPIO pin for the DS3231's interrupt signal (SQW).
#define BUZZER_PIN 4        ///< GPIO pin for the active buzzer.

// --- Timing Constants (in milliseconds) ---
#define LOOP_INTERVAL 100                  ///< The target interval for the main application loop.
#define DEBOUNCE_DELAY 50                  ///< Debounce delay for button interrupts.
#define FACTORY_RESET_HOLD_TIME 10000      ///< Time to hold the BOOT button for a runtime factory reset.
#define BOOT_FACTORY_RESET_HOLD_TIME 30000 ///< Time to hold the SNOOZE button at boot for a factory reset.
#define SETUP_CANCEL_DELAY 2000            ///< Duration to display the "Reset cancelled" message.
#define WEB_SERVER_STABILIZATION_DELAY 100 ///< Brief delay to allow the web server to stabilize after starting.
#define OFFLINE_MODE_MESSAGE_DELAY 5000    ///< Duration to display the "Offline Mode" message.
#define PREFERENCES_NAMESPACE "clock_config"
#define SAVE_DEBOUNCE_DELAY 5000 // 5 seconds

// --- Alarm Constants ---
#define ALARM_RESUME_DELAY_MS 5000    ///< Delay after boot before resuming a ringing alarm.
#define ALARM_AUTO_OFF_SECONDS 1800   ///< Time in seconds before a ringing alarm shuts off automatically (30 minutes).
#define STAGE1_DURATION_MS 10000      ///< Duration of the slow beeping stage of the alarm.
#define STAGE2_DURATION_MS 20000      ///< Duration of the fast beeping stage of the alarm.
#define SLOW_BEEP_ON_MS 200           ///< Duration of the "on" part of a slow beep.
#define SLOW_BEEP_OFF_MS 800          ///< Duration of the "off" part of a slow beep.
#define FAST_BEEP_ON_MS 150           ///< Duration of the "on" part of a fast beep.
#define FAST_BEEP_OFF_MS 150          ///< Duration of the "off" part of a fast beep.
#define SNOOZE_DISMISS_HOLD_TIME 3000 ///< Time in milliseconds to hold the button to dismiss a snoozed alarm.

// --- Display Constants (in pixels) ---
#define MARGIN 10                     ///< Default margin for screen layout.
#define CLOCK_SPRITE_WIDTH 343        ///< Width of the main time sprite (HH:MM).
#define CLOCK_SPRITE_HEIGHT 106       ///< Height of the main time sprite.
#define TOD_SPRITE_WIDTH 45           ///< Width of the AM/PM sprite.
#define TOD_SPRITE_HEIGHT 34          ///< Height of the AM/PM sprite.
#define SECONDS_SPRITE_WIDTH 70       ///< Width of the seconds sprite.
#define SECONDS_SPRITE_HEIGHT 50      ///< Height of the seconds sprite.
#define DAY_OF_WEEK_SPRITE_HEIGHT 50  ///< Height of the day of the week sprite.
#define DATE_SPRITE_HEIGHT 50         ///< Height of the date sprite.
#define TEMP_SPRITE_HEIGHT 50         ///< Height of the temperature sprite.
#define HUMIDITY_SPRITE_HEIGHT 50     ///< Height of the humidity sprite.
#define ALARM_SPRITE_WIDTH_PADDING 10 ///< Padding for the alarm text sprite.
#define ALARM_SPRITE_HEIGHT 50        ///< Height of the alarm text sprite.
#define ALARM_PROGRESS_BAR_HEIGHT 6   ///< Height of the dismiss progress bar.
#define ALARM_ICON_X 462              ///< X-coordinate of the alarm indicator icon.
#define ALARM_ICON_Y 5                ///< Y-coordinate of the alarm indicator icon.
#define ALARM_ICON_WIDTH 18           ///< Width of the alarm indicator icon's bounding box.
#define ALARM_ICON_HEIGHT 18          ///< Height of the alarm indicator icon's bounding box.

// --- Brightness Constants ---
const int BRIGHTNESS_MIN = 10;
const int BRIGHTNESS_MAX = 255;

#endif
