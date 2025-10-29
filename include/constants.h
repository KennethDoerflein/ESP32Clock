#ifndef CONSTANTS_H
#define CONSTANTS_H

// --- Pin Definitions ---
#define SNOOZE_BUTTON_PIN 5
#define BOOT_BUTTON_PIN 0
#define RTC_INT_PIN 2 // SQW pin from DS3231
#define BUZZER_PIN 4

// --- Timing Constants ---
#define LOOP_INTERVAL 100                  // Interval for the main loop in milliseconds
#define DEBOUNCE_DELAY 50                  // Debounce delay for button interrupts in milliseconds
#define FACTORY_RESET_HOLD_TIME 30000      // Hold time for factory reset in milliseconds
#define BOOT_FACTORY_RESET_HOLD_TIME 10000 // Hold time for factory reset at boot in milliseconds
#define SETUP_CANCEL_DELAY 2000            // Delay to show reset cancelled message
#define WEB_SERVER_STABILIZATION_DELAY 100 // Delay for web server stabilization
#define OFFLINE_MODE_MESSAGE_DELAY 5000    // Delay to show offline mode message

// --- Alarm Constants ---
#define ALARM_RESUME_DELAY_MS 5000
#define ALARM_AUTO_OFF_SECONDS 1800 // 30 minutes
#define STAGE1_DURATION_MS 10000    // 10 seconds of slow beeping
#define STAGE2_DURATION_MS 20000    // 20 seconds of fast beeping
#define SLOW_BEEP_ON_MS 200
#define SLOW_BEEP_OFF_MS 800
#define FAST_BEEP_ON_MS 150
#define FAST_BEEP_OFF_MS 150
#define SNOOZE_DISMISS_HOLD_TIME 3000 // 3-second hold to dismiss snooze

// --- Display Constants ---
#define MARGIN 10
#define CLOCK_SPRITE_WIDTH 343
#define CLOCK_SPRITE_HEIGHT 106
#define TOD_SPRITE_WIDTH 45
#define TOD_SPRITE_HEIGHT 34
#define SECONDS_SPRITE_WIDTH 70
#define SECONDS_SPRITE_HEIGHT 50
#define DAY_OF_WEEK_SPRITE_HEIGHT 50
#define DATE_SPRITE_HEIGHT 50
#define TEMP_SPRITE_HEIGHT 50
#define HUMIDITY_SPRITE_HEIGHT 50
#define ALARM_SPRITE_WIDTH_PADDING 20
#define ALARM_SPRITE_HEIGHT 50
#define ALARM_PROGRESS_BAR_HEIGHT 10
#define ALARM_ICON_X 462
#define ALARM_ICON_Y 5
#define ALARM_ICON_WIDTH 18
#define ALARM_ICON_HEIGHT 18

#endif // CONSTANTS_H