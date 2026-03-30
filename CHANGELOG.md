# Changelog — Seegrid Aging Room Environmental Monitoring System

All notable changes to this project are documented in this file.

---

## [v1.12] — 2026-03-29

### Added
- **Hardware Watchdog Timer** — 8-second watchdog armed via `wdt_enable(WDTO_8S)` in `setup()`. If the main loop stalls for any reason (network hang, SD deadlock, infinite loop), the Arduino automatically reboots
- **Watchdog Event Logging** — On every boot, once NTP syncs successfully, a timestamped entry is written to `EVENTS.txt` on the SD card, creating a permanent audit trail of all watchdog-triggered reboots
- **Watchdog Alerts Panel** — Web dashboard now shows a "Recent Watchdog Alerts" panel below System Status, displaying the 5 most recent entries from `EVENTS.txt`, polling every 60 seconds automatically
- **Clear Alerts button** — Calls `/clear-events` to delete `EVENTS.txt` and reset the log
- **Safe SD Eject** — "Prepare SD for Removal / Halt" button in System Status panel calls `/eject`, unmounts the SD card safely, displays `SD UNMOUNTED / SAFE TO UNPLUG` on the LCD, and halts the system in a safe loop until power is removed
- **Archive date range picker** — Archive tab redesigned from single-date to From/To date range. `downloadDateRange()` fetches all matching daily files across the selected range, combines them into a single merged CSV download with progress reporting
- **`/events` endpoint** — Serves `EVENTS.txt` from SD card as plain text
- **`/clear-events` endpoint** — Deletes `EVENTS.txt` from SD card
- **`/eject` endpoint** — Safely unmounts SD, updates LCD, halts system in watchdog-pet loop
- **ERR blink animation** — ERR state sensor dots and labels now visually pulse via CSS `@keyframes errorBlink` animation

### Changed
- **Sensor identity colors** — Changed to a colorblind-friendly palette: A=`#0072B2` (blue), B=`#E69F00` (yellow/amber), C=`#CC79A7` (pink), D=`#56B4E9` (light blue). Colors are consistent across status bar, chart lines, and chart legend
- **Archive tab** — Single calendar date picker replaced with From/To date range selector with progress feedback
- **Boot LCD hostname scroll** — Extended from 5 seconds to 10 seconds
- **NTP sync** — Watchdog is disarmed before `requestNtpTime()` and re-armed immediately after, preventing false reboots during the 3-second NTP timeout window
- **`wdt_reset()` coverage** — Called in: main `loop()` top, client connection inner loop, `/archive` file streaming, `serveFile()` data streaming, SD init failure loop, and all blocking sections of `handleButtonPress()` (5s hold, entry LED flash, adjustment loop, save confirmation, success screen)
- **Chart time axis** — Added `chartjs-adapter-date-fns` CDN for proper timestamp-based x-axis rendering
- **Threshold adjustment range** — `MIN_THRESHOLD` changed from 37 to -40, `MAX_THRESHOLD` changed from 47 to 80 in `config.h`

### Fixed
- Watchdog false reboot during button adjustment — all multi-second blocking loops in `handleButtonPress()` now call `wdt_reset()`
- Watchdog false reboot during SD file streaming — large file reads now pet the watchdog inline to prevent timeout during chart data delivery
- Watchdog false reboot during SD init failure — infinite LED blink loop now calls `wdt_reset()` to prevent reboot cascade

---

## [v1.11] — 2026-03-25

### Added
- **Time-Aligned Logging** — Sensor readings now logged on strict 5-minute clock boundaries (e.g., 12:00, 12:05)
- **180-Day Rolling Retention** — Data stored in daily files: `YYMMDD_T.csv` (temperature) and `YYMMDD_H.csv` (humidity)
- **Midnight Janitor** — Automated cleanup runs at 00:00 daily, deleting files exactly 180 days old
- **Dynamic Chart Stitching** — Web server combines the 7 most recent daily files into a single data stream for the dashboard
- **Archive Data tab** — Calendar picker UI for downloading specific historical 24-hour CSV files
- **`/cleanup` endpoint** — Hidden endpoint to delete legacy `temp.csv` and `humid.csv` from the SD card
- **Sensor status bar** — Shows live temperature in °C and °F per sensor with colored dot: green (OK), yellow (LOW/HIGH), red (ERR)
- **`/status` endpoint** — Returns label-prefixed sensor data: `A:21.3|OK,B:20.9|LOW,...`
- **`/sysinfo` endpoint** — Returns free RAM, uptime, SD status, last CSV write time, last NTP sync time
- **System Status panel** — RAM color-coded green/yellow/red, uptime, SD status, last write, last NTP sync. Polls every 30 seconds
- **`serveSystemInfo()`** — New C++ function in `storage.cpp`
- **Chart zoom and pan** — Scroll wheel zooms x-axis, click-drag pans, pinch for touch via `chartjs-plugin-zoom` and `hammerjs`
- **Reset Zoom button** — On both Temperature and Humidity chart tabs
- **Hover tooltip** — Shows all sensor values at the nearest time point (`mode: index`, `intersect: false`)
- **Legend click toggle** — Clicking a legend item hides/shows that dataset
- **Larger legend items** — `boxWidth: 24`, `padding: 16`, font size 13
- **Y-axis labels** — "Celsius (°C)" on temperature chart, "Humidity (%)" on humidity chart
- **X-axis formatting** — Timestamps rotated 45°, max 24 ticks to prevent crowding

### Changed
- `serveStatus()` updated from plain `OK/ERR` to label-prefixed `LABEL:TEMP|STATE` format
- `updateStatusBar()` keyed by sensor label (not array index) to prevent corruption when legend items are toggled
- `sysPanel` div placed correctly outside `<script>` tag in HTML to prevent browser parse errors

### Fixed
- `extern LiquidCrystal_I2C lcd` moved to file scope in `storage.cpp`
- DST month index: `epochToDateTime()` returns 0-indexed month; fixed by passing `month + 1` to `isDST()` in `tryNtpSync()`

---

## [v1.10] — 2026-03-22

### Fixed
- DST still reporting EST instead of EDT despite v1.5 algorithm fix
- Root cause: `epochToDateTime()` returns 0-indexed month (0=January, 2=March) but `isDST()` expects 1-indexed month (3=March)
- Fixed by passing `month + 1` to `isDST()` in `tryNtpSync()` in `network.cpp`
- System now correctly reports `DST Active: Yes (EDT)` and applies UTC-4 offset

---

## [v1.9] — 2026-03-22

### Fixed
- `extern LiquidCrystal_I2C lcd` moved from inside `initSDCard()` function body to file scope at top of `storage.cpp`

### Added
- Sensor status bar above chart tabs showing colored dot and OK/ERR label for each sensor (A, B, C, D)
- Chart legend dots turn red when a sensor is in ERR state, return to original color when OK
- `/status` endpoint returns comma-separated OK/ERR state for all four sensors
- `serveStatus()` function added to `storage.cpp` and `storage.h`
- `/status` endpoint added to request handler in `Aging_Room.ino`
- Status bar and legend colors poll `/status` every 30 seconds for live updates

### Changed
- Boot sequence standby delay reduced from 10 seconds to 3 seconds
- NTP fallback to `pool.ntp.org` added — `network.cpp` updated with `tryNtpSync()` helper

---

## [v1.8] — 2026-03-21

### Fixed
- Chart downsampling was hardcoded to every 12th row regardless of selected time range
- Downsampling now scales dynamically: 1 day = every reading, 3 days = every 6th, 5/7 days = every 12th

---

## [v1.7] — 2026-03-21

### Added
- `/threshold` endpoint returns current threshold value as plain text
- `pollThreshold()` polls every 30 seconds, updates chart threshold lines without page reload
- `serveThreshold()` added to `storage.cpp` and `storage.h`

### Fixed
- Chart margin corrected from hardcoded `3.0` to `5.0` to match `THRESHOLD_MARGIN` in `config.h`
- `threshold` JavaScript variable changed from `const` to `let` to allow live updates

---

## [v1.6] — 2026-03-21

### Added
- NTP fallback to public pool (`pool.ntp.org` / 216.239.35.0) if internal server `192.168.80.8` times out
- `tryNtpSync()` helper function handles a single NTP server attempt, returns true/false
- Serial Monitor reports which server responded or reports timeout on both

---

## [v1.5] — 2026-03-20

### Fixed
- DST detection broken — system was reporting EST instead of EDT
- Replaced `isDST()` back-calculation with `nthWeekdayOfMonth()` helper using Tomohiko Sakamoto's algorithm
- DST check now uses UTC hour before applying timezone offset
- `isDST()` signature updated to accept `hour` parameter for correct 2:00 AM transition handling

### Changed
- `network.h` updated to reflect new `isDST(int year, int month, int day, int hour)` signature
- `nthWeekdayOfMonth()` prototype added to `network.h`

---

## [v1.4] — 2026-03-19

### Fixed
- Sensor retry delays reduced from 500ms to 100ms in `readSensors()`

---

## [v1.3] — 2026-03-19

### Fixed
- Temperature threshold showing as NaN or 0 on boot when EEPROM has never been written
- `isnan()` check added to `initSensors()` so NaN values are caught and reset to default
- EEPROM now written immediately on first boot if no valid threshold is found
- Threshold adjustment loop now correctly uses `MIN_THRESHOLD` and `MAX_THRESHOLD` constants

### Changed
- Default temperature threshold changed to **42°C**
- Threshold adjustment range changed to **37°C – 47°C** (42°C ±5°C)
- Alert margin changed to **±5°C** (previously ±3°C)

### Refactored
- Project split from single `.ino` file into multi-file architecture

---

## [v1.2] — 2026-02-21

### Fixed
- Resolved HTTP 413 errors caused by request buffer being too small
- Fixed SHA256 authentication salt+password hashing order

### Changed
- Request buffer size increased to 1024 bytes
- Authentication now uses salted SHA256: `SHA256(SALT + PASSWORD)`

---

## [v1.1] — 2026-02-04

### Added
- Salted SHA256 password hashing to replace plain SHA256
- `AUTH_SALT` constant in `config.h`
- Comprehensive memory architecture documentation in README

### Changed
- Authentication credentials moved fully to `config.h` Flash constants

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
