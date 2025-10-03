// utils.cpp

#include "utils.h" // Include the header file

// Define the char arrays
const char *dayNames[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

const char *monthNames[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

void printChipInfo()
{
  Serial.println("\n--- ESP32-S3-N16R8 Board Info ---");
  Serial.printf("Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("CPU Cores: %d\n", ESP.getChipCores());
  Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  if (psramFound())
  {
    Serial.printf("PSRAM Size: %d MB\n", ESP.getPsramSize() / (1024 * 1024));
  }
  Serial.println("------------------------------------");
}