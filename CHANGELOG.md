# Changelog — Seegrid Aging Room Environmental Monitoring System

All notable changes to this project are documented in this file.

---

## [v1.5] — 2026-03-20

### Fixed
- DST detection broken — system was reporting EST instead of EDT on March 20 2026
- Replaced unreliable `isDST()` back-calculation logic with `nthWeekdayOfMonth()` helper using Tomohiko Sakamoto's algorithm to correctly find the 2nd Sunday of March and 1st Sunday of November
- DST check now uses UTC hour before applying timezone offset, preventing off-by-one errors on transition days
- `isDST()` signature updated to accept `hour` parameter instead of `weekday` for correct 2:00 AM transition handling
- Local time re-parsed after offset applied so Serial Monitor displays correct local time

### Changed
- `network.h` updated to reflect new `isDST(int year, int month, int day, int hour)` signature
- `nthWeekdayOfMonth()` prototype added to `network.h`

---

## [v1.4] — 2026-03-19

### Fixed
- Sensor retry delays reduced from 500ms to 100ms in `readSensors()` to prevent fast red blink appearing slow when no sensors are attached

---

## [v1.3] — 2026-03-19

### Fixed
- Temperature threshold showing as NaN or 0 on boot when EEPROM has never been written
- Added `isnan()` check to `initSensors()` so NaN values are caught and reset correctly
- EEPROM now written immediately on first boot if no valid threshold is found, preventing repeated NaN on subsequent boots
- Threshold adjustment loop now correctly uses `MIN_THRESHOLD` and `MAX_THRESHOLD` constants instead of hardcoded 20/50 values

### Changed
- Default temperature threshold changed to **42°C**
- Threshold adjustment range changed to **37°C – 47°C** (42°C ±5°C)
- Alert margin changed to **±5°C** (previously ±3°C)
- Threshold adjustment now cycles 37 → 38 → ... → 47 → 37 using config constants
- README pin configuration table updated to reflect actual hardware pins (A:40, B:41, C:30, D:31, Green:47, Red:46, Button:50)
- README static IP updated to `192.168.48.20`
- README serial baud rate corrected to 115200
- README threshold troubleshooting section updated to reflect NaN fix and new valid range

### Refactored
- Project split from single `.ino` file into multi-file architecture:
  - `config.h` — all constants and library includes
  - `auth.cpp` / `auth.h` — salted SHA256 authentication
  - `network.cpp` / `network.h` — connection tracking, NTP, time
  - `sensors.cpp` / `sensors.h` — DHT22 reading, LEDs, button
  - `display.cpp` / `display.h` — LCD initialization and updates
  - `storage.cpp` / `storage.h` — SD card and web server

---

## [v1.2] — 2026-02-21

### Fixed
- Resolved HTTP 413 errors caused by request buffer being too small
- Fixed SHA256 authentication salt+password hashing order (salt prepended before password)

### Changed
- Request buffer size increased to 1024 bytes
- Authentication now uses salted SHA256: `SHA256(SALT + PASSWORD)`

---

## [v1.1] — 2026-02-04

### Added
- Salted SHA256 password hashing to replace plain SHA256
- Rainbow table attack protection via per-installation unique salt
- `AUTH_SALT` constant in `config.h`
- Comprehensive memory architecture documentation in README
- Password hash generation instructions in README

### Changed
- Authentication credentials moved fully to `config.h` Flash constants
- Improved password security documentation and deployment guidelines

---

## [v1.0] — 2026-01-25

### Added
- Four-sensor DHT22 temperature and humidity monitoring
- 20×4 I2C LCD display with rotating temperature and humidity views
- Boot sequence with LED test and LCD pixel test
- Web dashboard with interactive Chart.js charts (1, 3, 5, 7 day views)
- CSV data logging to SD card every 5 minutes
- NTP time synchronization with automatic Eastern Time DST adjustment
- Basic SHA256 web authentication
- IP-based connection rate limiting (8 global, 3 per IP)
- EEPROM persistence for temperature threshold
- Physical button interface for threshold adjustment
- LED status indicators (solid green, slow red blink, fast red blink)
- HTTP 503 response when connection limits are reached
- 5-second request timeout to prevent slowloris attacks
