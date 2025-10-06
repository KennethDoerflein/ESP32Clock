// ntp.cpp

#include "ntp.h"
#include "sensors.h"

// Define the NTP server addresses. Using multiple servers provides redundancy.
const char *NTP_SERVER = "time.nist.gov";
const char *BACKUP_NTP_SERVER = "time.cloudflare.com";
const char *BACKUP2_NTP_SERVER = "us.pool.ntp.org";
// Define the timezone string for US Eastern Time.
// This POSIX-compliant string handles Standard Time (EST) and Daylight Saving Time (EDT).
const char *TZ_INFO = "EST5EDT,M3.2.0,M11.1.0"; // US Eastern Time Zone

bool getNTPData(struct tm &timeinfo)
{
  // Configure the ESP32's time client with the NTP servers.
  configTime(0, 0, NTP_SERVER, BACKUP_NTP_SERVER, BACKUP2_NTP_SERVER);
  // Set the timezone environment variable. This allows getLocalTime to apply the correct offset.
  setenv("TZ", TZ_INFO, 1);
  tzset(); // Apply the new timezone setting.

  // Attempt to get the local time. This function contacts the NTP servers.
  // It populates the `timeinfo` struct and returns true on success.
  return getLocalTime(&timeinfo);
}

bool syncTime()
{
  // --- Robust NTP Sync with Exponential Backoff and Jitter ---
  const int maxRetries = 25;              // Maximum number of sync attempts.
  const unsigned long baseDelayMs = 1000; // Initial delay before retrying.
  const unsigned long maxDelayMs = 30000; // Maximum delay between retries.
  const int jitterMaxMs = 1000;           // Maximum random jitter to add to the delay.

  unsigned long currentDelay = baseDelayMs; // Start with the base delay.
  struct tm timeinfo;                       // Struct to hold the received time.

  // Loop through the retry attempts.
  for (int i = 1; i <= maxRetries; i++)
  {
    Serial.printf("Fetching NTP time (Attempt %d/%d)...\n", i, maxRetries);

    // If getNTPData succeeds, it populates `timeinfo`.
    if (getNTPData(timeinfo))
    {
      // Convert the C `tm` struct to an `RTClib::DateTime` object.
      // Note: `tm_year` is years since 1900, `tm_mon` is 0-11.
      DateTime receivedTime(
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);

      // Compensate for potential network and processing delays by adding a small offset.
      TimeSpan compensation(1); // 1 second
      DateTime adjustedTime = receivedTime + compensation;

      // Update the hardware RTC with the adjusted time.
      RTC.adjust(adjustedTime);

      Serial.print("RTC synchronized with NTP time (compensated): ");
      char timeStr[20];
      sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
              adjustedTime.year(),
              adjustedTime.month(),
              adjustedTime.day(),
              adjustedTime.hour(),
              adjustedTime.minute(),
              adjustedTime.second());
      Serial.println(timeStr);
      return true; // Success, exit the function.
    }

    // If the sync failed and we haven't exhausted retries, wait and try again.
    if (i < maxRetries)
    {
      // Add a random jitter to the delay to prevent multiple devices from retrying in lockstep.
      unsigned long jitter = random(jitterMaxMs + 1);
      unsigned long totalDelay = currentDelay + jitter;

      Serial.printf("Failed to obtain time. Retrying in %.2f seconds...\n", totalDelay / 1000.0);
      delay(totalDelay);

      // Double the delay for the next attempt (exponential backoff).
      currentDelay *= 2;

      // Cap the delay at the maximum defined value.
      if (currentDelay > maxDelayMs)
      {
        currentDelay = maxDelayMs;
      }
    }
  }

  // If all retries fail, report the error.
  Serial.println("Failed to sync time with NTP server after all retries.");
  return false;
}