# ESP32-S3 WiFi Clock

This repository contains the firmware for a feature-rich, Wi-Fi connected smart clock powered by an ESP32-S3 microcontroller. It features a large 3.5" color TFT display, a web-based interface for easy configuration, automatic time synchronization, and environmental sensing.

## Features

- **Large Color Display**: A 3.5" 480x320 color TFT display provides a clear and vibrant user interface.
- **Web-Based Configuration**: A mobile-friendly web UI allows for easy setup and configuration without needing to re-flash the firmware.
- **WiFi & AP Mode**: Connects to your local WiFi network or starts its own Access Point (`ESP32-Clock-Setup`) if credentials are not set.
- **Automatic Time Sync**: An onboard Real-Time Clock (DS3231) with battery backup keeps accurate time, synchronized daily with NTP internet servers.
- **Environment Sensing**: A BME280 sensor provides live temperature and humidity readings on the display.
- **Smart Brightness Control**: The display backlight can be controlled manually via the web UI or set to an automatic day/night schedule.
- **Over-the-Air (OTA) Updates**: Update the clock's firmware directly from the web interface, either by uploading a binary file or pulling the latest release directly from GitHub.
- **Persistent Storage**: All settings (WiFi credentials, display preferences) are saved to the ESP32's internal flash storage using LittleFS.

---

## Hardware Requirements

| Component           | Description                                                          |
| ------------------- | -------------------------------------------------------------------- |
| **Microcontroller** | **ESP32-S3-WROOM-1 N16R8** Development Board (16MB Flash, 8MB PSRAM) |
| **Display**         | **3.5" ILI9488 TFT LCD** Display Module (480x320) with SPI interface |
| **Real-Time Clock** | **DS3231** RTC Module with a CR2032 battery for backup               |
| **Sensor**          | **BME280** Temperature, Humidity, and Pressure Sensor Module (I2C)   |
| **Power Supply**    | A reliable 5V USB-C power supply                                     |

## Wiring

The components are connected using the I2C and SPI buses.

| Component                  | Pin       | ESP32-S3 GPIO | Notes                 |
| -------------------------- | --------- | ------------- | --------------------- |
| **3.5" ILI9488 TFT (SPI)** | VCC       | VIN           | Connect to 5V input   |
|                            | GND       | GND           | Common ground         |
|                            | CS        | GPIO 10       | Chip Select           |
|                            | RST       | GPIO 11       | Reset                 |
|                            | DC        | GPIO 12       | Data/Command          |
|                            | MOSI      | GPIO 13       | Data In               |
|                            | MISO      | UNUSED        | Data Out              |
|                            | SCK       | GPIO 14       | Clock                 |
|                            | LED       | GPIO 6        | Backlight (PWM)       |
| **DS3231 & BME280 (I2C)**  | VCC       | 3.3V          | Shared 3.3V           |
|                            | GND       | GND           | Shared ground         |
|                            | SDA       | GPIO 8        | Shared data line      |
|                            | SCL       | GPIO 9        | Shared clock line     |
| **Active Buzzer**          | +         | GPIO 4        | Any free GPIO         |
|                            | -         | GND           | Common ground         |
| **Stop/Snooze Button**     | One leg   | GPIO 5        | Use INPUT_PULLUP mode |
|                            | Other leg | GND           | Common ground         |

## Software Setup

This project is developed using **PlatformIO**.

### Libraries

The following libraries are required and are automatically managed by PlatformIO via the `platformio.ini` file:

- `bodmer/TFT_eSPI`
- `bblanchon/ArduinoJson`
- `adafruit/RTClib`
- `adafruit/Adafruit BME280 Library`
- `esphome/ESPAsyncWebServer-esphome`
- `esphome/AsyncTCP-esphom`

### Build Instructions

1.  **Clone the Repository**:
    ```bash
    git clone https://github.com/KennethDoerflein/ESP32Clock.git
    cd ESP32Clock
    ```
2.  **Open in PlatformIO**: Open the project folder in VS Code with the PlatformIO extension installed.
3.  **Build & Upload**: Use the PlatformIO "Build" and "Upload" commands to compile and flash the firmware to your ESP32-S3.

---

## Usage Guide

### First-Time Setup

1.  On the first boot, or if WiFi credentials are not configured, the clock will start a Wi-Fi Access Point (AP). The SSID will be **`ESP32-Clock-Setup`**.
2.  Connect to this network from your phone or computer.
3.  Open a web browser and navigate to the clock's static IP address, which is **`http://192.168.4.1`**. This address is fixed when the clock is in AP mode.

### Web Interface

The web interface provides access to all the clock's settings.

#### 1. WiFi Configuration

- Enter the SSID and password for the selected network and click **"Save & Connect"**.
- The device will save the credentials and restart to connect to your WiFi network. On success, the display will show the new IP address assigned by your router. You can now access the web interface from this new IP address on your local network.

#### 2. Clock Settings

- **Auto Brightness**: Check this box to enable a time-based schedule (bright during the day, dim at night).
- **Manual Brightness**: If Auto Brightness is disabled, use this slider to set a fixed brightness level.
- **24-Hour Format**: Switch the clock display between 12-hour (with AM/PM) and 24-hour format.
- **Use Celsius (°C)**: Switch the temperature display between Celsius and Fahrenheit.

#### 3. Firmware Update

- **Manual Upload**: Click "Choose File" to select a `firmware.bin` file from your computer and click "Upload" to start the update.
- **GitHub Update**: Click the "Check for Updates from GitHub" button to automatically fetch and install the latest firmware release from this repository.

---

## Project Roadmap

This project is broken down into phases to prioritize a functional base clock before implementing more complex features.

### Phase 1: Core Functionality (Complete: Untested)

- [x] **Hardware Integration:** Connect ESP32-S3 with ILI9488 display, DS3231 RTC, and BME280 sensor.
- [x] **Display Logic:**
  - [x] Render time, date, day of the week.
  - [x] Render temperature and humidity.
  - [x] Implement basic status messages.
- [x] **Timekeeping:**
  - [x] Initialize RTC and keep time accurately.
  - [x] Connect to Wi-Fi and sync time with an NTP server on startup.
  - [x] Implement a daily automatic time sync.
- [x] **Basic Configuration:**
  - [x] Store and retrieve settings from LittleFS.
  - [x] Set up initial Wi-Fi credentials via a captive portal.

### Phase 2: Web Interface & Advanced Configuration (In Progress)

- [x] **UI/UX Enhancements:**

  - [x] Create a single-page application (SPA) feel for the web UI.
  - [x] Add a "Reboot Device" button to the web interface.
  - [ ] Add button to change hostname to the web interface.
  - [ ] Add option to reset to defaults to the web interface.
  - [ ] Implement WiFi network scanning for easier setup.

- [x] **Settings Management:**
  - [x] **Display Options:**
    - [x] Toggle between 12/24-hour format.
    - [x] Toggle between Celsius/Fahrenheit.
    - [x] Control manual brightness via a slider.
    - [x] Enable/disable automatic brightness scheduling.
  - [ ] **Timezone Configuration:**
    - [ ] Add a dropdown to select the timezone.
- [ ] **Firmware Updates:**
  - [ ] Provide OTA updates via file upload.
  - [ ] Provide OTA updates directly from GitHub releases.

### Phase 3: Alarms & User Interaction (In Progress)

- [x] **Alarm Management:**
  - [x] **Backend:**
    - [x] Define a data structure to store multiple alarms.
    - [x] Save and load alarms from persistent storage.
    - [x] Implement logic to trigger an alarm at the set time.
  - [x] **Web Interface:**
    - [x] Create a UI to edit alarms.
    - [ ] Create a UI to add, and delete alarms.
    - [x] Allow setting the time for each alarm.
    - [x] Allow setting which days of the week an alarm repeats.
    - [ ] Significantly increase 5 alarm limit
  - [ ] **Hardware Interaction:**
    - [x] Use a physical button to snooze a ringing alarm.
    - [x] Use a long-press on the physical button to permanently dismiss a ringing alarm for the day.
    - [ ] Implement a factory reset by long-pressing the boot button.
- [ ] **Audio Feedback:**
  - [ ] Implement progressive alarm volume (ramping).
  - [ ] Add different sound options for the alarm.
- [ ] **Display Feedback:**
  - [x] Show a visual indicator on the display when an alarm is set.
  - [x] Show a "ringing" screen when an alarm is active.
  - [ ] Add a snooze countdown
