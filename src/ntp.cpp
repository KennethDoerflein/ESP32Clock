// ntp.cpp
#include "ntp.h"
#include "sensors.h"

const char *NTP_SERVER = "time.nist.gov";
const char *BACKUP_NTP_SERVER = "time.cloudflare.com";
const char *BACKUP2_NTP_SERVER = "us.pool.ntp.org";
const char *TZ_INFO = "EST5EDT,M3.2.0,M11.1.0"; // For US Eastern Time Zone

bool getNTPData(struct tm &timeinfo)
{
  // Configure time with NTP servers and apply the TZ string for automatic DST
  configTime(0, 0, NTP_SERVER, BACKUP_NTP_SERVER, BACKUP2_NTP_SERVER); // Set to UTC initially
  setenv("TZ", TZ_INFO, 1);
  tzset();

  // getLocalTime() populates the timeinfo struct and returns true on success.
  return getLocalTime(&timeinfo);
}

bool syncTime()
{
  const int maxRetries = 25;
  const unsigned long baseDelayMs = 1000; // Start with a 1-second base delay
  const unsigned long maxDelayMs = 30000; // Cap the delay at 30 seconds
  const int jitterMaxMs = 1000;           // Add up to 1 second of random jitter

  unsigned long currentDelay = baseDelayMs; // Initialize delay for the first retry
  struct tm timeinfo;                       // Declare struct to hold the time info

  for (int i = 1; i <= maxRetries; i++)
  {
    Serial.printf("Fetching NTP time (Attempt %d/%d)...\n", i, maxRetries);

    // Returns true on success and fills timeinfo.
    if (getNTPData(timeinfo))
    {
      // Create a DateTime object from the received NTP time
      DateTime receivedTime(
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);

      // Add 1-second compensation for processing delay
      TimeSpan compensation(1);
      DateTime adjustedTime = receivedTime + compensation;

      // Success! Set the RTC.
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
      return true; // Exit successfully
    }

    // If it failed and it's not the last attempt, wait before the next retry.
    if (i < maxRetries)
    {
      // Add random jitter to the current delay
      unsigned long jitter = random(jitterMaxMs + 1); // random(0, 1000)
      unsigned long totalDelay = currentDelay + jitter;

      Serial.printf("Failed to obtain time. Retrying in %.2f seconds...\n", totalDelay / 1000.0);
      delay(totalDelay);

      // Double the delay for the next attempt (exponential backoff)
      currentDelay *= 2;

      // Cap the delay at the defined maximum
      if (currentDelay > maxDelayMs)
      {
        currentDelay = maxDelayMs;
      }
    }
  }

  Serial.println("Failed to sync time with NTP server after all retries.");
  return false;
}