# Changelog — Seegrid Aging Room Environmental Monitoring System

All notable changes to this project are documented in this file.

---

## [v1.23] — 2026-05-26

### Changed
- **RS485 architecture changed from push to poll** — UNO nodes no longer transmit on a timer. The Mega now controls the bus entirely, sending `GET:SKIT\n` and `GET:CAM\n` poll requests sequentially. Each UNO listens for its own address and responds once before returning to receive mode. Bus contention is now physically impossible regardless of UNO resets or power events.
- **Mega MAX485 DE+RE moved from GND to pin 34** — the Mega MAX485 was previously receive-only (DE+RE tied to GND). Pin 34 is now firmware-controlled: LOW at idle (receive), briefly HIGH during poll request transmission, then immediately LOW again.
- **RS485 poll interval clock-aligned to 5-minute boundaries** — polling fires at :00, :05, :10 ... :55 every hour, matching the CSV write schedule. On boot, an immediate poll fires as soon as NTP time is valid, giving fresh data right away. Subsequent polls follow the clock boundary schedule.
- **`RS485_POLL_INTERVAL` define removed** — replaced with epoch-based clock alignment logic in `readRS485()`. `lastPollTime` variable also removed.
- **Skit and Camera UNO sketches rewritten as poll responders** — `loop()` now listens on SoftwareSerial for the Mega poll request and responds with one packet. No timers, no stagger logic, no `randomSeed()`. `rs485.listen()` added to `setup()` to activate SoftwareSerial receive.
- **`config.h` comment corrected** — RS485 pin comment updated from "DE+RE tied to GND" to "DE+RE on pin 34".

### Fixed
- **Skit and Camera web page status showing all dashes** — `pollStatus()` JavaScript in `serveRoomPage()` had mismatched closing braces causing the entire script to fail silently. Fixed brace count in the `parts.forEach()` callback.

### Hardware Changes Required
- **Mega MAX485:** move DE+RE wire from GND to **Mega pin 34**
- **Both UNO MAX485 boards:** add RO wire from MAX485 RO pin to **UNO pin 8** (previously unconnected)

### Notes
- The root cause of the intermittent RS485 failure was bus contention: both UNOs transmitting simultaneously after one UNO reset and lost its stagger offset. The poll architecture eliminates this permanently — only the addressed UNO ever transmits, and only when the Mega asks.
- The Mega DE+RE wire move and the UNO RO wire are both required for the poll architecture to function.

---

## [v1.22] — 2026-05-19

### Fixed
- **RAM recovered after admin page additions** — `handleAdminDeleteAll()` file name buffer reduced from `char names[40][16]` to `char names[20][16]`, saving 320 bytes. Additional `F()` macro coverage applied to all simple HTTP header strings in `serveSkitStatus()`, `serveCameraStatus()`, `serveSkitThresholdTemp()`, `serveSkitThresholdHumid()`, `serveCameraSysInfo()`, and related endpoint handlers (~620 bytes moved from RAM to Flash). RAM recovered from 1.3 KB back to ~1.9 KB.

### Notes
- RAM is currently in the yellow zone (~1.9 KB). Safe to run. Monitor over time; if it drifts below 1.5 KB, another F() pass or further buffer reduction is warranted.
- `char names[20][16]` supports up to 20 files per delete-all pass. With the 180-day rolling purge, active file count stays well below this limit.

---

## [v1.21] — 2026-05-19

### Added
- **Admin page: Delete All Data Files** — new "Delete All" button on `/admin` page. Deletes all date-prefixed CSV data files in one action. Protected active files (`temp.csv`, `humid.csv`, `SK_T.csv`, `SK_H.csv`, `CA_T.csv`, `CA_H.csv`) are excluded. Confirmation dialog required before execution. New route `/admin/delete-all` added to Aging Room router. New function `handleAdminDeleteAll(EthernetClient &client)` added to `storage.cpp` and declared in `storage.h`.
- **Admin page: SD card used space** — total SD used space (sum of all file sizes) now displayed at the top of the `/admin` page. Note: free space is not available with the stock `SD.h` library; only used space is computable by walking the SD root.
- **RS485 bad-packet rejection guards** added to `readRS485()` in `sensors.cpp` — after parsing incoming Skit and Camera packets, values are validated against physical bounds (5-50 C, 1-99% RH). Out-of-range values are rejected and logged to Serial; no CSV write occurs. Prevents corrupt or truncated packets from logging physically impossible values.

### Fixed
- **ERR packet timestamp fix** — `lastSkitReceive` / `lastCamReceive` now update on ERR packets as well as valid data packets. Prevents the timeout watchdog from logging a second ERR to the CSV in the same cycle when the UNO itself already reported a sensor failure.

---

## [v1.20] — 2026-05-05

### Changed
- **Skit Room and Camera Room node boards corrected to Arduino UNO** — previously documented and labeled as Arduino Nano. Both transmitter nodes are Arduino UNO boards. Boot messages updated to `Skit Room UNO Ready` and `Camera Room UNO Ready`
- **RS485 baud rate changed from 115200 to 9600** — `RS485_BAUD` in `config.h` changed to `9600`. SoftwareSerial on Arduino UNO is unreliable at 115200; 9600 is stable for the short DHT22 packets transmitted. Mega `Serial1` updated to match
- **RS485 DE pin control added to both UNO transmitter sketches** — DE+RE previously hardwired to 5V (always enabled), causing both UNOs to drive the bus simultaneously and collide. DE+RE now connected to UNO pin 10 and controlled in firmware: pulled HIGH only for the duration of packet transmission, then immediately pulled LOW to release the bus. This is required for correct multi-transmitter RS485 operation
- **Camera Room transmit interval corrected** — `TRANSMIT_INTERVAL` was incorrectly set to `360000UL` (6 minutes) in one revision; restored to `420000UL` (7 minutes). Skit Room remains 6 minutes
- **Midnight grid lines added to Skit Room and Camera Room charts** — Temperature and Humidity charts on `/skit` and `/camera` now draw bold dark vertical grid lines at midnight boundaries on multi-day views, matching the Aging Room chart behavior
- **RAM optimization — `F()` macro applied to all remaining bare string literals** in `network.cpp` and `storage.cpp`. Affected strings: all connection limit debug messages, all NTP request/response/timeout messages, DST status strings, date/time separator characters, `sendServiceUnavailable()` full HTML response, SD card init messages. Estimated ~250 bytes of RAM freed

### Fixed
- **RS485 wiring note corrected** — Previous documentation stated transmit-only nodes could use DE hardwired HIGH. This is incorrect when multiple transmitters share the same bus — corrected in README and wiring diagram

### Notes
- **Wiring change required on both UNOs:** Move the DE+RE wire from VCC (5V) to digital pin 10 on each UNO
- **Mega wiring unchanged:** DE+RE on Mega MAX485 remains tied to GND (receive-only)

---

## [v1.19] — 2026-04-27

### Removed
- **`/cleanup` endpoint** — deleted from `Aging_Room.ino` router. The endpoint deleted `temp.csv` and `humid.csv` which were legacy flat files from before v1.11. Since v1.11 the system writes date-prefixed daily files (`YYMMDD_T.csv`, `YYMMDDST.csv`, etc.) and no flat files are created. The endpoint was a no-op on any system running v1.11 or later. File deletion is now handled entirely through the `/admin` page.

---

## [v1.18] — 2026-04-27

### Added
- **Camera Room web server** — Full `/camera`, `/camera/status`, `/camera/sysinfo`, `/camera/threshold/temp`, `/camera/threshold/humid` endpoints added. Mirrors Skit Room architecture. Camera Room UNO transmits on `CAM:` prefix, 7-minute interval.
- **Skit Room web server** — Full `/skit`, `/skit/status`, `/skit/sysinfo`, `/skit/threshold/temp`, `/skit/threshold/humid` endpoints. Skit Room UNO transmits on `SKIT:` prefix, 6-minute interval.
- **RS485 receive on Mega Serial1** — `readRS485()` in `sensors.cpp` parses `SKIT:` and `CAM:` prefixed packets from Serial1 (pins 18/19). DE+RE on Mega MAX485 tied to GND (receive-only, never transmits).
- **RS485 timeout watchdog** — if no packet is received from Skit within 6.5 minutes or Camera within 7.5 minutes, an `ERR` entry is logged to the respective CSVs.
- **Camera Room boot offset** — Camera UNO uses a 45-second boot offset (`lastTransmit = millis() - (TRANSMIT_INTERVAL - 45000UL)`) to stagger transmissions and prevent RS485 bus collisions with the Skit UNO.

---

## [v1.17] — 2026-04-20

### Added
- **Admin page (`/admin`)** — hidden endpoint (no nav link) listing all files on the SD card with individual delete buttons. Protected active files cannot be deleted. Accessible only to authenticated users.
- **`handleAdminDelete()`** — deletes a named file from SD via POST to `/admin/delete?file=FILENAME`. Active data files are blocked from deletion.

---

## [v1.16] — 2026-04-13

### Added
- **`/eject` endpoint** — gracefully halts CSV writes and flushes buffers before SD card removal. LCD displays "Safe to Remove SD" confirmation.

---

## [v1.15] — 2026-04-06

### Added
- **180-day rolling log purge** — `purgeOldLogs()` runs on boot and deletes any date-prefixed CSV files that are exactly 180 days old. Prevents SD card from filling up over time.

---

## [v1.14] — 2026-03-30

### Added
- **Connection rate limiting** — max 8 simultaneous tracked connections. Returns HTTP 503 with Retry-After header when limit is exceeded. Tracked in `connectionTracker[]` array in `network.cpp`.

---

## [v1.13] — 2026-03-29

### Changed
- **RS485 timeout values increased** — Skit timeout increased to 6 minutes, Camera timeout increased to 7 minutes to match actual UNO transmit intervals and eliminate false ERR entries.

---

## [v1.12] — 2026-03-28

### Added
- **Watchdog timer** — 8-second hardware watchdog via `avr/wdt.h`. `wdt_reset()` called in main loop and all long-running operations.

---

## [v1.11] — 2026-03-27

### Changed
- **CSV logging switched to date-prefixed daily files** — from single flat `temp.csv`/`humid.csv` to `YYMMDD_T.csv` / `YYMMDD_H.csv` (Aging Room), `YYMMDDST.csv` / `YYMMDDSH.csv` (Skit), `YYMMDDCT.csv` / `YYMMDDCH.csv` (Camera). One file per room per day. Chart endpoint reads and concatenates multiple files for multi-day views.

---

## [v1.10] — 2026-03-26

### Fixed
- **DST detection month index bug** — `nthWeekdayOfMonth()` was using 1-indexed months but receiving 0-indexed values from time struct. Fixed using Tomohiko Sakamoto's algorithm.

---

## [v1.9] — 2026-03-25

### Added
- **Data downsampling for multi-day chart views** — chart endpoint now returns downsampled data for 3d/5d/7d views to keep HTTP response size manageable. Sampling ratio scales dynamically by range (1d=1x, 3d=6x, 5/7d=12x)

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
- Default threshold to 42 C, adjustment range to -40 C to 80 C, alert margin to +/-5 C

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
