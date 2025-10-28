# ESP32-S3 WiFi Clock

This repository contains the firmware for a feature-rich, Wi-Fi connected smart clock powered by an ESP32-S3 microcontroller. It features a large 3.5" color TFT display, a web-based interface for easy configuration, automatic time synchronization, and environmental sensing.

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Software Setup](#software-setup)
- [Usage Guide](#usage-guide)
  - [First-Time Setup](#first-time-setup)
  - [Web Interface](#web-interface)
  - [Physical Button Functions](#physical-button-functions)
  - [Factory Reset](#factory-reset)
- [Features and Settings in Detail](#features-and-settings-in-detail)
- [Project Roadmap](#project-roadmap)

## Features

- **Large Color Display**: A 3.5" 480x320 color TFT display provides a clear and vibrant user interface.
- **Web-Based Configuration**: A mobile-friendly web UI allows for easy setup and configuration without needing to re-flash the firmware.
- **WiFi & AP Mode**: Connects to your local WiFi network or starts its own Access Point (`Clock-Setup`) if credentials are not set.
- **Automatic Time Sync**: An onboard Real-Time Clock (DS3231) with battery backup keeps accurate time, synchronized daily with NTP internet servers.
- **Temperature Sensors**: A BME280 sensor for ambient temperature and humidity, and a sensor within the DS3231 for time-drift compensation.
- **Smart Brightness Control**: The display backlight can be controlled manually via the web UI or set to an automatic day/night schedule.
- **Over-the-Air (OTA) Updates**: Update the clock's firmware directly from the web interface, either by uploading a binary file or pulling the latest release directly from GitHub.
- **Persistent Storage**: All settings (WiFi credentials, display preferences) are saved to the ESP32's internal flash storage using LittleFS.
- **mDNS Support**: Access the clock's web interface using a human-readable name (e.g., `ESP32Clock_XXXXXX.local`) instead of an IP address.
- **Physical Factory Reset**: Two hardware-based methods to reset the device to its default settings.

---

## Hardware Requirements

| Component           | Description                                                          |
| ------------------- | -------------------------------------------------------------------- |
| **Microcontroller** | **ESP32-S3-WROOM-1 N16R8** Development Board (16MB Flash, 8MB PSRAM) |
| **Display**         | **3.5" ILI9488 TFT LCD** Display Module (480x320) with SPI interface |
| **Real-Time Clock** | **DS3231** RTC Module with a LIR2032 battery for backup              |
| **Sensor**          | **BME280** Temperature, Humidity, and Pressure Sensor Module (I2C)   |
| **Power Supply**    | A reliable 5V USB-C power supply                                     |

## Wiring

The components are connected using the I2C and SPI buses.

| Component                  | Pin       | ESP32-S3 GPIO | Notes                 |
| -------------------------- | --------- | ------------- | --------------------- |
| **3.5" ILI9488 TFT (SPI)** | VCC       | 3.3V          | Connect to 3.3V pin   |
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
|           DS3231           | SQW       | GPIO 2        | Interrupt for alarms  |
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

1.  On the first boot, or if WiFi credentials are not configured/valid, the clock will start a Wi-Fi Access Point (AP). The SSID will be **`Clock-Setup`**.
2.  Connect to this network from your phone or computer.
3.  Open a web browser and navigate to the clock's static IP address, which is **`http://192.168.4.1`**. This address is fixed when the clock is in AP mode.

### Web Interface

The web interface provides access to all the clock's settings. Once connected to your WiFi network, you can access the web interface by navigating to the IP address shown on the clock's display or by using its mDNS address (e.g., `http://ESP32Clock_XXXXXX.local`).

The web interface is organized into several pages, accessible from the main control panel:

- **Configure WiFi**: For connecting to your local network and setting a hostname.
- **Alarms**: For setting and managing up to 5 alarms.
- **Settings**: For adjusting display, time, and brightness settings.
- **System**: For system-level actions like updates, reboots, and viewing system information.

### Physical Button Functions

The physical button (connected to GPIO 5) serves multiple purposes depending on the context:

-   **Normal Operation**: A short press cycles through the available display pages (e.g., Clock, Info).
-   **Alarm Ringing**:
    -   A **short press** will **snooze** the alarm for the duration specified in the web interface.
    -   **Pressing and holding** the button will **dismiss** the alarm. The required hold time can be configured in the web interface.
-   **Alarm Snoozed**:
    -   **Pressing and holding** the button for 3 seconds will end the snooze and dismiss the alarm.

### Factory Reset

There are three ways to perform a factory reset:

1.  **Via the Web Interface**: Navigate to the "System" page and click the "Factory Reset" button.
2.  **Boot-Time Reset**:
    -   Disconnect the clock from power.
    -   Press and hold the **Snooze button** (GPIO 5).
    -   Reconnect the power while still holding the button.
    -   Continue holding for **10 seconds**. The screen will display a confirmation message.
3.  **Runtime Reset**:
    -   While the clock is running normally, press and hold the **Boot button** (GPIO 0) on the ESP32 board for **30 seconds**.

---
## Features and Settings in Detail

### WiFi Page

This page allows you to connect the clock to your local WiFi network.

-   **Scan for Networks**: Click the "Scan" button to see a list of available WiFi networks. Clicking on a network name will automatically fill in the SSID field.
-   **SSID & Password**: Manually enter your network's name and password.

-   **Save & Reboot**: Click to save the credentials to the device's persistent memory. The clock will then automatically reboot and connect to the configured network.
-   **Hostname**: You can set a custom hostname for the device on your network. This also serves as the mDNS address (e.g., `http://your-hostname.local`).

### Alarms Page

-   **Alarm Management**: The clock supports up to 5 configurable alarms. For each alarm, you can:
    -   **Enable/Disable**: Toggle the alarm on or off.
    -   **Set Time**: Configure the hour and minute.
    -   **Set Repeat Days**: Choose which days of the week the alarm should be active.
-   **Snooze Duration**: Set the number of minutes the alarm will wait before ringing again after the snooze button is pressed.
-   **Dismiss Duration**: Configure how many seconds the physical button must be held down to dismiss a ringing alarm completely.

### Settings Page

This page is divided into two tabs: "General" and "Display".

#### General Tab

-   **Brightness Settings**:
    -   **Auto Brightness**: Enables a schedule-based brightness adjustment, with configurable start/end times and day/night brightness levels.
    -   **Manual Brightness**: When Auto Brightness is disabled, a single slider allows you to set a fixed brightness level.
-   **Time and Display Format**:
    -   **24-Hour Format**: Toggles the time display between 12-hour (e.g., 3:45 PM) and 24-hour (e.g., 15:45) formats.
    -   **Use Celsius (°C)**: Switches the temperature display between Celsius and Fahrenheit.
    -   **Flip Display Orientation**: Rotates the display by 180 degrees.
-   **Timezone**: Select your local timezone from a dropdown list.

#### Display Tab

This tab provides a set of color pickers to customize the appearance of nearly every element on the clock face.

-   **Customizable Elements**: Background, Time, AM/PM, Seconds, Day of the Week, Date, Temperature, and Humidity.
-   **Reset to Defaults**: A button is provided to revert all color settings to their original values.

### System Page

This page contains tools for firmware updates and system-level commands.

#### Firmware Update

-   **Manual Upload**: Update the firmware by selecting a compiled `.bin` file from your computer and clicking "Upload."
-   **GitHub Update**: The clock can automatically check for the latest release on this GitHub repository and perform an update if a newer version is available. A status message will indicate if you are on the latest version or if an update is available.

#### System Actions

-   **Reboot Device**: Safely restarts the clock.
-   **Factory Reset**: Erases all stored settings (including WiFi credentials, alarms, and display preferences) and reboots the device.
-   **Factory Reset (Keep WiFi)**: Erases all settings *except* for the saved WiFi credentials, then reboots. This is useful for resetting display or alarm settings without needing to re-configure the network connection.

#### Serial Log Viewer

-   **Availability**: This feature is only enabled in development builds of the firmware.
-   **Functionality**: Provides a real-time stream of the ESP32's serial output directly in the web browser, which is useful for debugging.

#### System Information

The System page also displays a live feed of the clock's internal statistics, which can be useful for diagnostics:

-   **Free Heap**: The amount of available memory.
-   **Uptime**: How long the device has been running since its last reboot.
-   **WiFi RSSI**: The signal strength of the WiFi connection.
-   **ESP32 Core Temperature**: The internal temperature of the main processor.

### A Note on Temperature Sensors

This project utilizes two temperature sensors:

1.  **BME280**: This is the primary sensor for ambient room temperature and humidity, and its readings are what's shown on the main display.
2.  **DS3231**: The Real-Time Clock module contains its own internal temperature sensor. This is used by the RTC to compensate for temperature variations and maintain accurate timekeeping. It is not displayed on the clock face.

---

## Project Roadmap

This project is broken down into phases to prioritize a functional base clock before implementing more complex features.

### Completed Features

- [x] **Core Functionality**
  - [x] **Hardware Integration:** ESP32-S3 with ILI9488 display, DS3231 RTC, and BME280 sensor.
  - [x] **Display Logic:** Time, date, day of the week, temperature, humidity, and status messages.
  - [x] **Timekeeping:** RTC initialization, Wi-Fi time sync with NTP, and daily automatic updates.
  - [x] **Basic Configuration:** Settings storage in LittleFS and Wi-Fi setup via a captive portal.
- [x] **Web Interface & Advanced Configuration**
  - [x] **UI/UX:** Single-page application feel, reboot button, and Wi-Fi network scanning.
  - [x] **Settings Management:** 12/24-hour format, Celsius/Fahrenheit toggle, manual and automatic brightness.
  - [x] **Firmware Updates:** OTA updates via file upload and directly from GitHub releases.
- [x] **Alarms & User Interaction**
  - [x] **Alarm Management:** Backend logic for multiple alarms, persistent storage, and UI for editing time and repeat days.
  - [x] **Hardware Interaction:** Physical button for snooze and dismiss functions.
  - [x] **Audio Feedback:** Progressive alarm volume.
  - [x] **Display Feedback:** On-screen alarm indicator.
  - [x] **Hardware Interaction:** Implement a factory reset via a long-press of the boot button.
  - [x] **Display Feedback:** Show a "ringing" screen when an alarm is going off.
  - [x] **Display Feedback:** Add a visual indicator for how long to hold the snooze button.
- [x] **UI/UX Enhancements**
  - [x] Add an option to reset to defaults in the web interface.
  - [x] Improve TOD and seconds centering.
  - [x] Improve the appearance of the temperature unit and degree sign.
  - [x] Change display colors from webui
  - [x] flip display orientation from webui
- [x] **Firmware Updates**
  - [x] Warn the user if they try to leave the page during an update.
  - [x] Improve version handling (don't update version.h).

### Work in Progress

- [ ] **General Problems**
  - [ ] wifi disconnects after a few hours
- [x] **Alarms & User Interaction**
  - [x] **Alarm Management:**
    - [x] Let user set snooze duration
    - [x] Let user set hold to dismiss alarm duration
  - [x] **Display Feedback:**
    - [x] Show a snooze countdown.
- [ ] **Settings Management**
  - [x] Add a dropdown to select the timezone.
  - [ ] Add an option to change the default page.

### Future Plans

- [ ] **Code Cleanup**

  - [ ] **Cleanup & Organization:** Replace "magic numbers," remove unused `#include` directives, and standardize naming.
  - [ ] **Code Refactoring:** Rewrite messy code using modern C++ practices, resolve conflicting logic, reduce complexity, and consolidate redundant variables.

  - [ ] **Documentation:** Update the `README.md` and code comments.

- [ ] **Add Weather Page**
  - [ ] Get longitude and latitude from a zip code and store it in the filesystem.
  - [ ] Use location data to get weather from NOAA.
  - [ ] Update weather information every ~10 minutes.
  - [ ] Add a "Good Morning" page that displays the weather after an alarm goes off.
