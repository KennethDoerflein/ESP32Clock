#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>

// Expose global tft object if you want one instance
extern TFT_eSPI tft;

// Setup and init
void setupDisplay();

// Drawing functions
void drawClock(const char *timeStr);
void drawDate(const char *dateStr);
void drawSensors(float temp, float humidity);

#endif
