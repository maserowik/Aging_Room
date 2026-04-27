# Changelog — Seegrid Aging Room Environmental Monitoring System

All notable changes to this project are documented in this file.

---

## [v1.18] — 2026-04-27

### Added
- **Hidden admin page** at `/admin` — SD card file manager accessible by URL only, no navigation link on any page
  - Lists all files on the SD card with human-readable sizes (B / KB / MB)
  - Download button per file — reuses existing `/archive` endpoint
  - Delete button per file with browser `confirm()` dialog
  - Auto-redirects back to admin page after successful delete
  - Dark theme to visually distinguish from normal dashboard pages
  - Shows current timestamp in subtitle
- **`/admin/delete?file=FILENAME` endpoint** — deletes named file from SD card
  - Active data files (`temp.csv`, `humid.csv`, `SK_T.csv`, `SK_H.csv`, `CA_T.csv`, `CA_H.csv`) are blocked from deletion with a 403 response
  - Returns 404 if file does not exist
  - Returns 200 with auto-refresh meta tag on success
- **`serveAdminPage()` and `handleAdminDelete()`** added to `storage.cpp` and declared in `storage.h`
- **Admin routes** added to `Aging_Room.ino` router — `GET /admin/delete?file=` matched before `GET /admin` to prevent prefix collision

### Notes
- All admin HTML strings use `F()` macro — zero additional RAM usage at runtime
- Peak stack usage during admin page serve: ~70 bytes (released on function return)
- Admin page is protected by the same Basic Auth as all other endpoints

---

## [v1.17] — 2026-04-27

### Added
- **Skit Room dashboard** at `/skit` — full interactive dashboard for the Skit Room RS485 node
  - Temperature and humidity charts with 1/3/5/7 day range selector
  - Live sensor status bar showing temp (°C/°F) and humidity (% RH) with OK/LOW/HIGH/ERR states
  - System Status panel showing RAM, uptime, SD status, Last Receive timestamp, NTP sync
  - Threshold Adjustment panel with Up/Down buttons for temp and humid thresholds
  - Archive Data tab with date range picker for CSV download
  - Offline detection banner with auto-reconnect
- **Camera Room dashboard** at `/camera` — identical feature set to Skit Room dashboard
- **`serveRoomPage()`** — consolidated private function in `storage.cpp` that generates the sub-room dashboard HTML, parameterized by room name, URL base, file prefix, chart colors, thresholds, and active nav state. Both `serveSkitPage()` and `serveCameraPage()` are thin wrappers around this function
- **Skit Room API endpoints:**
  - `GET /skit/status` — `TEMP:value|state,HUMID:value|state`
  - `GET /skit/sysinfo` — RAM, uptime, SD, last receive, NTP sync
  - `GET /skit/threshold/temp` — current Skit Room temp threshold
  - `GET /skit/threshold/humid` — current Skit Room humid threshold
  - `POST /skit/threshold/temp?v=XX.X` — update and persist Skit Room temp threshold to EEPROM
  - `POST /skit/threshold/humid?v=XX.X` — update and persist Skit Room humid threshold to EEPROM
  - `GET /skit/temp.csv` — Skit Room temperature data (last 7 days, stitched from daily files)
  - `GET /skit/humid.csv` — Skit Room humidity data (last 7 days, stitched from daily files)
- **Camera Room API endpoints** — identical set under `/camera/` prefix
- **Navigation bar on all pages** — Aging Room, Skit Room, Camera Room buttons on every dashboard; active room highlighted darker
- **Skit Room Nano sketch** (`Skit_Room/Skit_Room.ino`) — Arduino Nano RS485 transmitter
  - Reads DHT22 on pin 4 every 6 minutes
  - Transmits `SKIT:21.5,45.2\n` packet over SoftwareSerial RS485
  - Sends `SKIT:ERR,ERR\n` on sensor failure
  - Debug output on hardware Serial at 115200 baud
- **Camera Room Nano sketch** (`Camera_Room/Camera_Room.ino`) — identical to Skit Room sketch with `CAM:` prefix
- **RS485 receive parsing** in `sensors.cpp` (`readRS485()`) — parses incoming `SKIT:` and `CAM:` packets from Serial1, extracts temp and humid floats, updates `tSkit`, `hSkit`, `tCam`, `hCam` globals, updates `lastSkitReceive` / `lastCamReceive` timestamps
- **EEPROM persistence** for Skit Room temp/humid thresholds and Camera Room temp/humid thresholds — all survive power outages
- **Daily file logging** for Skit Room (`YYMMDDST.csv`, `YYMMDDSH.csv`) and Camera Room (`YYMMDDCT.csv`, `YYMMDDCH.csv`) in `appendCsvData()`
- **180-day purge** extended to cover Skit and Camera daily files in `purgeOldLogs()`

### Changed
- **`serveFile()`** extended to support virtual filenames `SK_T.csv`, `SK_H.csv`, `CA_T.csv`, `CA_H.csv` — stitches the last 7 days of Skit/Camera daily files the same way the Aging Room virtual CSVs work
- **Navigation on root page** updated to include Skit Room and Camera Room buttons
- **`storage.h`** updated with all new function declarations

### Notes
- **Temporary wiring:** Skit Room sensor is currently wired directly to Mega pin 19 (Serial1 RX) bypassing RS485 hardware while MAX485 modules are on order. Data is receiving correctly. No firmware changes required when modules arrive — just swap the wiring
- **RS485 module clarification:** Must use MAX485 TTL module (~$1–2). RS232-to-RS485 converters are NOT compatible with Arduino — they use ±12V RS232 levels which will damage Arduino pins. RS232-to-RS485 converters are for PC/PLC use only
- **Transmit-only nodes:** Because Skit and Camera Nanos only ever transmit, a module with DE hardwired HIGH will work on the Nano side. The Mega receive side needs proper RE control or a module with RE hardwired LOW

---

## [v1.16] — 2026-04-09

### Added
- **Humidity in status bar** — The sensor status bar now displays live humidity values when the Humidity tab is active (`XX.X% RH` per sensor), and returns to showing temperature (`XX.X°C / XX.X°F`) when any other tab is active. The last received status payload is cached in `lastStatus` so the bar re-renders immediately on tab switch without waiting for the next poll cycle
- **`activeTab` tracking** — A JavaScript `activeTab` variable is set on every `showTab()` call, allowing `updateStatusBar()` to know which data type to display without re-fetching from the Arduino

### Changed
- **`/status` endpoint response format** — Now returns three pipe-separated fields per sensor: `LABEL:TEMP|STATE|HUMID` (e.g. `A:22.1|OK|45.3`). Previously returned `LABEL:TEMP|STATE` only. Humidity is appended as a raw float; `ERR` is returned if the humidity read failed
- **`serveStatus()` in `storage.cpp`** — Now externs `hA`, `hB`, `hC`, `hD` and appends each sensor's humidity reading after the threshold state field

---

## [v1.15] — 2026-04-09

### Added
- **Midnight grid lines** — Both Temperature and Humidity charts now draw a bold dark vertical grid line at each midnight boundary on multi-day views
- **Dynamic x-axis label format** — Tick labels switch format based on selected range: single-day = time only; multi-day = date + time

### Changed
- **Range dropdown defaults** — Explicit `value=` attributes on every `<option>` tag
- **SD read buffer increased** — `buf[64]` → `buf[128]` in `serveFile()`
- **`appendCsvData()` watchdog coverage** — `wdt_reset()` added after each SD file close

---

## [v1.14] — 2026-04-05

### Added
- **`wdt_disable()/wdt_enable()` around `appendCsvData()`** — SD card writes fully bracketed with watchdog disable/re-arm
- **Additional `wdt_reset()` checkpoints in `serveRootPage()`** — Ten checkpoint calls distributed throughout HTML generation

---

## [v1.13] — 2026-04-02

### Added
- **Offline detection banner** — Yellow "SYSTEM OFFLINE" banner with auto-reconnect every 4 seconds
- **`safeFetch()` wrapper** — All fetch calls go through safeFetch for consistent error handling
- **Staggered poll timers** — `updateCharts` 307s, `pollStatus` 29s, `pollSysInfo` 31s, `pollThreshold` 37s, `pollEvents` 53s
- **`bootUp()` async init sequence** — Sequential await on page load before arming repeating timers
- **`EVENTS.txt` missing guard** — Returns empty 200 OK instead of 404 on fresh install

### Changed
- **Authentication restored** — Full inline auth check with Authorization header capture
- **All poll functions skip when offline**

---

## [v1.12] — 2026-03-29

### Added
- **Hardware Watchdog Timer** — 8-second watchdog via `wdt_enable(WDTO_8S)`
- **Watchdog Event Logging** — Timestamped entries in `EVENTS.txt` on every boot
- **Watchdog Alerts Panel** — Dashboard shows 5 most recent reboot events
- **Clear Alerts button** — Calls `/clear-events`
- **Safe SD Eject** — `/eject` endpoint unmounts SD and halts safely
- **Archive date range picker** — From/To range with combined CSV download
- **`/events`, `/clear-events`, `/eject` endpoints**
- **ERR blink animation** — CSS `@keyframes errorBlink` on ERR state indicators

### Changed
- **Sensor identity colors** — Colorblind-friendly palette: A=`#0072B2`, B=`#E69F00`, C=`#CC79A7`, D=`#56B4E9`
- **Threshold adjustment range** — `MIN_THRESHOLD` → -40, `MAX_THRESHOLD` → 80

---

## [v1.11] — 2026-03-25

### Added
- **Time-Aligned Logging** — 5-minute clock boundary logging
- **180-Day Rolling Retention** — Daily files with midnight janitor
- **Dynamic Chart Stitching** — Last 7 days combined for dashboard
- **`/cleanup`, `/status`, `/sysinfo` endpoints**
- **System Status panel**, **sensor status bar**, **chart zoom/pan**

---

## [v1.10] — 2026-03-22

### Fixed
- DST month index bug — `epochToDateTime()` returns 0-indexed month; fixed by passing `month + 1` to `isDST()`

---

## [v1.9] — 2026-03-22

### Added
- Sensor status bar, `/status` endpoint, NTP fallback to `pool.ntp.org`

### Fixed
- `extern LiquidCrystal_I2C lcd` moved to file scope in `storage.cpp`

---

## [v1.8] — 2026-03-21

### Fixed
- Chart downsampling now scales dynamically by range (1d=1x, 3d=6x, 5/7d=12x)

---

## [v1.7] — 2026-03-21

### Added
- `/threshold` endpoint, `pollThreshold()` live dashboard updates

---

## [v1.6] — 2026-03-21

### Added
- NTP fallback to `pool.ntp.org` / `216.239.35.0`

---

## [v1.5] — 2026-03-20

### Fixed
- DST detection — replaced back-calculation with `nthWeekdayOfMonth()` using Tomohiko Sakamoto's algorithm

---

## [v1.4] — 2026-03-19

### Fixed
- Sensor retry delays reduced from 500ms to 100ms

---

## [v1.3] — 2026-03-19

### Fixed
- Temperature threshold NaN on first boot — added `isnan()` guard in `initSensors()`

### Changed
- Default threshold → 42°C, adjustment range → -40°C to 80°C, alert margin → ±5°C

### Refactored
- Project split from single `.ino` into multi-file architecture

---

## [v1.2] — 2026-02-21

### Fixed
- HTTP 413 errors from small request buffer
- SHA256 salt+password hashing order

---

## [v1.1] — 2026-02-04

### Added
- Salted SHA256 password hashing, `AUTH_SALT` constant

---

## [v1.0] — 2026-01-25

### Added
- Four-sensor DHT22 monitoring, LCD display, web dashboard, CSV logging, NTP, SHA256 auth, connection rate limiting, EEPROM threshold persistence, physical button interface, LED status indicators
