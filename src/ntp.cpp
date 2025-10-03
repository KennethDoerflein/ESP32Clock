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
  const int maxRetries = 10;
  struct tm timeinfo; // Declare struct once to hold the time info

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

      // The small delay between receiving the NTP packet and setting the RTC can
      // cause the clock to be nearly a full second behind.
      // A common and effective solution is to add a 1-second compensation
      // to ensure the RTC is set for the second that is about to begin.
      TimeSpan compensation(1); // Create a TimeSpan of exactly 1 second
      DateTime adjustedTime = receivedTime + compensation;

      // Success! Set the RTC with the precisely compensated time.
      RTC.adjust(adjustedTime);

      Serial.print("RTC synchronized with NTP time (compensated): ");
      char timeStr[20];
      // Use the adjustedTime object to print what was actually set
      sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
              adjustedTime.year(),
              adjustedTime.month(),
              adjustedTime.day(),
              adjustedTime.hour(),
              adjustedTime.minute(),
              adjustedTime.second());
      Serial.println(timeStr);
      return true; // Exit the function on success
    }

    // If it failed and it's not the last attempt, wait before the next retry.
    if (i < maxRetries)
    {
      Serial.println("Failed to obtain time. Retrying in 5 seconds...");
      delay(5000); // Wait for 5 seconds
    }
  }

  // If the loop completes without a successful sync
  Serial.println("Failed to sync time with NTP server after all retries.");
  return false;
}