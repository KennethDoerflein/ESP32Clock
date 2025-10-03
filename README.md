# ESP32-S3 Smart Alarm Clock

A feature-rich, Wi-Fi connected smart alarm clock powered by the ESP32-S3 microcontroller. This project leverages the performance of the S3 and its ample PSRAM to drive a large, 3.5" color display and a modern web-based interface for easy setup.

<!-- ![Project Photo Placeholder](https://via.placeholder.com/600x400.png?text=ESP32-S3+Smart+Clock) -->

---

## Features

- **High-Performance Graphics:** A smooth, custom GUI on a 3.5" 480x320 color TFT display.
- **Web-Based Configuration:** Fully configurable via a mobile-friendly Web UI. No need to re-flash the firmware to change settings.
- **Automatic Time Sync:** Onboard Real-Time Clock (DS3231) with battery backup keeps perfect time, synchronized daily with NTP internet servers for ultimate accuracy.
- **Multiple Alarms:** Set and manage multiple alarms, each with its own schedule.
- **Progressive Wake-Up:** Gentle wake-up option with a progressively louder alarm sound.
- **Environment Sensing:** Live temperature and humidity display from a BME280 sensor.
- **Smart Dimming:** Display automatically dims based on the time of day to be easy on the eyes at night.
- **Over-the-Air (OTA) Updates:** Update the clock's firmware over Wi-Fi without ever needing a USB cable.
- **Persistent Storage:** Alarms, Wi-Fi credentials, and user settings are saved to the 16MB of onboard flash.

---

## Hardware

This project is built with the following specific components:

- **Microcontroller:** **ESP32-S3-WROOM-1 N16R8** Development Board. This powerful module features:
  - 16MB of Flash for storing the application, web files, and fonts.
  - 8MB of PSRAM, critical for high-performance graphics buffering.
  - Integrated Wi-Fi and Bluetooth LE.
- **Display:** **3.5" MSP3521 TFT LCD Touch Display Module (480x320)**, using an ILI9488 driver.
- **Real-Time Clock:** **DS3231** RTC Module with a CR2032 battery for backup.
- **Sensor:** **BME280** Temperature, Humidity, and Pressure Sensor Module.
- **Input:** Pushbutton(s) for physical interaction (snooze, dismiss, cycle views).
- **Audio:** A small buzzer or speaker for the alarm sound.
- **Power Supply:** A reliable 5V USB-C power supply.

---

## Wiring Diagram

| Component                  | Pin       | ESP32-S3 GPIO | Notes                 |
| -------------------------- | --------- | ------------- | --------------------- |
| **3.5" ILI9488 TFT (SPI)** | VCC       | VIN           | Connect to 5V input   |
|                            | GND       | GND           | Common ground         |
|                            | CS        | GPIO 10       | Chip Select           |
|                            | RST       | GPIO 11       | Reset                 |
|                            | DC        | GPIO 12       | Data/Command          |
|                            | MOSI      | GPIO 13       | Data In               |
|                            | MISO      | GPIO 21       | Data OUT              |
|                            | SCK       | GPIO 14       | Clock                 |
|                            | LED       | GPIO 2        | Backlight (PWM)       |
| **DS3231 & BME280 (I2C)**  | VCC       | 3.3V          | Shared 3.3V           |
|                            | GND       | GND           | Shared ground         |
|                            | SDA       | GPIO 8        | Shared data line      |
|                            | SCL       | GPIO 9        | Shared clock line     |
| **Active Buzzer**          | +         | GPIO 4        | Any free GPIO         |
|                            | -         | GND           | Common ground         |
| **Stop/Snooze Button**     | One leg   | GPIO 5        | Use INPUT_PULLUP mode |
|                            | Other leg | GND           | Common ground         |

## Software & Setup

This project is developed using PlatformIO.

### Required Libraries:

- **`TFT_eSPI by Bodmer`**
- `ESPAsyncWebServer`
- `AsyncTCP`
- `ArduinoJson`
- `RTClib by Adafruit`
- `Adafruit BME280 Library`

<!-- ### Configuration

1.  On first boot, the clock will start a Wi-Fi Access Point (AP).
2.  Connect to the "ESP32-Clock-Setup" Wi-Fi network.
3.  Navigate to `192.168.4.1` in your browser.
4.  From the web UI, provide your home Wi-Fi credentials.
5.  The clock will restart, connect to your network, and begin normal operation. All further configuration can be done by accessing the clock's IP address on your local network. -->

---

## Project Roadmap

This project is broken down into phases to prioritize a functional base clock before implementing more complex features.

### Phase 1: Core Clock & Display

_Goal: Get a beautiful, accurate, and self-sufficient clock running._

- [x] **Hardware:** Integrate the MSP3521 display with the ESP32-S3 using the `TFT_eSPI` library.
- [x] **Timekeeping:** Integrate the DS3231 RTC to keep time without power.
- [ ] **Connectivity:**
  - [ ] Connect to Wi-Fi
  - [x] Perform an initial time sync with an NTP server.
  - [x] Perform time sync every day at 3 AM
- [ ] **Display Logic:**
  - [x] Display the current time and date.
  - [ ] Implement the "click to switch views" logic.
  - [ ] Display temperature and humidity from the BME280.
- [ ] **System:** Set up Over-the-Air (OTA) updates to make future development easier.

---

### Phase 2: Alarms & User Interaction

_Goal: Add comprehensive alarm functionality._

- [ ] **Core Alarm Logic:**
  - [ ] Allow a single alarm to be set in the code.
  - [ ] Push button to snooze.
  - [ ] Hold button to disable alarm until the next day.
- [ ] **Advanced Alarms:**
  - [ ] Allow multiple alarms.
  - [ ] Implement progressive (ramping) alarm volume.

---

### Phase 3: Web Interface & Configuration

_Goal: Make the clock fully configurable without needing to re-flash._

- [ ] **Web Server:** Build the core `ESPAsyncWebServer` UI.
- [ ] **Wi-Fi Setup:** Create the initial Access Point for setting Wi-Fi credentials.
- [ ] **Configuration Pages:**
  - [ ] Set and manage alarms through the Web UI.
  - [ ] Configure timezone and DST settings.
  - [ ] Change time/temp format and other display preferences.
  - [ ] Add an option to disable auto-dimming.
- [ ] **Persistence:** Save all user settings (Wi-Fi, alarms, preferences) to the ESP32's flash memory.

---
