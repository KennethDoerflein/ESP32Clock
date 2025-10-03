#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Include custom files
#include "config.h"
#include "sensors.h"
#include "display.h"
#include "utils.h" // Assuming monthNames and RTC object are defined here or in config.h
#include "ntp.h"

// --- Configuration ---
const bool USE_24_HOUR_FORMAT = false; // Set to true for 24-hour, false for 12-hour

// --- Globals for timers ---
unsigned long prevClockMillis = 0;
unsigned long prevSensorMillis = 0;
const unsigned long CLOCK_UPDATE_INTERVAL = 1000;   // 1 second
const unsigned long SENSOR_UPDATE_INTERVAL = 10000; // 10 seconds
bool hasSyncedToday = false;                        // Flag to track daily NTP sync

void setup()
{
  Serial.begin(115200);
  setupSensors();
  setupDisplay();

  // Display a loading message on the screen so the user knows it's working
  drawClock("Setup");
  drawDate("Connecting WiFi");

  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Update the on-screen message before syncing
  drawDate("Syncing Time");

  // Sync time once after connecting to WiFi
  syncTime();
}

void loop()
{
  unsigned long now = millis();

  // --- Clock Update Logic ---
  // Reads from the RTC every second to update the display
  if (now - prevClockMillis >= CLOCK_UPDATE_INTERVAL)
  {
    prevClockMillis = now;

    // Now we read the time from the RTC, which has been correctly set
    DateTime t = RTC.now();

    // --- Daily NTP Sync Logic ---
    // At 3 AM, if we haven't synced for the day, call the sync function.
    if (t.hour() == 3 && !hasSyncedToday)
    {
      Serial.println("Performing daily 3 AM time sync...");
      syncTime();
      hasSyncedToday = true; // Mark as synced to prevent multiple calls
    }

    // Reset the flag once the 3 AM hour has passed.
    // This makes it ready for the next day.
    if (t.hour() == 4)
    {
      hasSyncedToday = false;
    }

    char timeStr[12]; // Increased size for "HH:MM:SS AM"
    if (USE_24_HOUR_FORMAT)
    {
      // 24-Hour Format
      sprintf(timeStr, "%02d:%02d:%02d", t.hour(), t.minute(), t.second());
    }
    else
    {
      // 12-Hour Format
      int hour12 = t.hour() % 12;
      if (hour12 == 0)
      {
        hour12 = 12; // Handle midnight (0) and noon (12)
      }
      const char *ampm = t.hour() < 12 ? "AM" : "PM";
      // Note: "%2d" prints the hour without a leading zero. Use "%02d" if you prefer one.
      sprintf(timeStr, "%2d:%02d:%02d %s", hour12, t.minute(), t.second(), ampm);
    }
    drawClock(timeStr);

    char dateStr[20];
    // Ensure monthNames array is accessible here
    sprintf(dateStr, "%s-%d", monthNames[t.month() - 1], t.day());
    drawDate(dateStr);
  }

  // --- Sensor Update Logic ---
  // Runs every 10 seconds
  if (now - prevSensorMillis >= SENSOR_UPDATE_INTERVAL)
  {
    prevSensorMillis = now;
    // float temp = readTemperature();
    // float hum = readHumidity();
    // drawSensors(temp, hum);
  }
}
