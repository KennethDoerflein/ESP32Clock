# ESP32Clock Unit Test Suites

This directory contains the unit tests for the ESP32Clock project.

The test framework uses [Unity](http://www.throwtheswitch.org/unity) via PlatformIO for embedded/C++ testing and Python's standard `unittest` framework for build-time and OTA signing scripts.

## Test Suites Overview

### 1. Embedded C++ Tests (PlatformIO Test Runner / Unity)

| Test Suite | Path | Description |
| :--- | :--- | :--- |
| **test_alarm** | `test/test_alarm/test_alarm.cpp` | Comprehensive coverage of the `Alarm` class: default state, hour/minute clamping, ID & enabled states, snooze mechanics, DST spring forward skipped hours, DST fall back duplicate ring prevention, biweekly parity, and dismiss behavior. |
| **test_config** | `test/test_config/test_config.cpp` | Directly tests `ConfigManager` singleton methods: default constants, enabled pages order, alarm replacement and vector manipulation, alarm summary calculations, first-snoozed search, and persistent settings getters/setters. |
| **test_constants** | `test/test_constants/test_constants.cpp` | Invariant checks for hardware GPIO pinouts, buzzer progressive ramping and beeping cadences, UI sprite geometry bounds, brightness limits, and safe-mode boot thresholds. |
| **test_firmware_verifier** | `test/test_firmware_verifier/test_firmware_verifier.cpp` | Cryptographic verification tests for `FirmwareVerifier`: NIST SHA-256 test vectors (empty input, known strings), streaming hash calculation context, hex string parsing (valid, invalid lengths, invalid characters), and public key format verification. |
| **test_lock_guard** | `test/test_lock_guard/test_lock_guard.cpp` | Multi-core synchronization tests for `LockGuard` and `RecursiveLockGuard`: null pointer safety, RAII acquisition and release, and nested recursive mutex re-entrancy. |
| **test_scripts** | `test/test_scripts/test_scripts.cpp` | C++ test validating build-time version injection: ensures `FIRMWARE_VERSION` is non-empty and follows semantic or development version formatting conventions (`v*` or `dev-*`). |
| **test_sensor** | `test/test_sensor/test_sensor.cpp` | Sensor calculations and temperature conversions: zero-offset preservation, zero-humidity protection against `log(0)` / `NaN`, physical cooling/warming humidity compensation curves, boundary clamping, and bidirectional Celsius/Fahrenheit conversion precision. |
| **test_time_manager** | `test/test_time_manager/test_time_manager.cpp` | Directly exercises `TimeManager` static formatting helpers: 12-hour and 24-hour time formatting, AM/PM calculation, month and day-of-week string conversions, seconds formatting, and `NextAlarmTime` strict weak ordering. |
| **test_utils** | `test/test_utils/test_utils.cpp` | Utility routines: RGB565 color conversions for primary and UI palette colors, hex case insensitivity, boundary conditions, and RFC 1123 hostname validation rules. |
| **test_weather_service** | `test/test_weather_service/test_weather_service.cpp` | Weather parsing and data representations: 16-point wind direction cardinal mapping, 22.5-degree boundaries and angular wrapping, WMO weather code translations, RFC 3986 URL encoding, and whole-word presence matching. |

### 2. Build-Time and OTA Script Tests (Python `unittest`)

| Test File | Path | Description |
| :--- | :--- | :--- |
| **test_ota_scripts** | `test/test_ota_scripts.py` | Exercises build-time and OTA signing helpers: Ed25519 public key C-array generation, header file updates, SHA-256 firmware hash calculations with NIST vectors, firmware artifact path resolution, private key parsing and validation, and development version string formatting. |

## Running Tests

### Running Python Tests (Host Machine)
To run the build and OTA script tests on your local machine:
```bash
python test/test_ota_scripts.py
```
Or via the test runner shim:
```bash
python -m unittest discover -s test
```

### Running Embedded C++ Tests (PlatformIO)
To run the C++ test suites on physical ESP32-S3 hardware connected via USB serial:
```bash
pio test -e esp32s3_n16r8
```
To run a specific test suite:
```bash
pio test -e esp32s3_n16r8 -f test_sensor
```
