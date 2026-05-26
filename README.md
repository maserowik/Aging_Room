# Seegrid Aging Room Environmental Monitoring System

An Arduino Mega-based multi-room temperature and humidity monitoring system for industrial aging room environments. Reads four DHT22 sensors on the Aging Room, receives RS485 sensor data from Skit Room and Camera Room UNO nodes, logs all data to an SD card, and serves interactive web dashboards per room with real-time alerts.

---

## Table of Contents

1. [How It Works](#how-it-works)
2. [System Architecture](#system-architecture)
3. [RS485 Network](#rs485-network)
4. [Requirements](#requirements)
5. [File Overview](#file-overview)
6. [Installation](#installation)
7. [Setting Your Authentication Password](#setting-your-authentication-password)
8. [Memory Architecture and Data Persistence](#memory-architecture-and-data-persistence)
9. [Web Interface URL Reference](#web-interface-url-reference)
10. [Usage](#usage)
11. [Adjusting the Temperature Threshold](#adjusting-the-temperature-threshold)
12. [Security Features](#security-features)
13. [Data Logging](#data-logging)
14. [Configuration Constants](#configuration-constants)
15. [Troubleshooting](#troubleshooting)
16. [Version History](#version-history)

---

## How It Works

Once running, the system enters a continuous monitoring loop that:

- **Reads all four DHT22 sensors** every 2 seconds and updates the LCD display. The display rotates through sensor zones automatically.
- **Polls RS485 sensor nodes** every 5 minutes on strict 5-minute clock boundaries. The Mega sends `GET:SKIT\n` then `GET:CAM\n` sequentially. Each UNO responds with one packet (`SKIT:21.5,45.2\n` or `CAM:21.5,45.2\n`) and returns to listen mode. Only one device transmits at a time — bus contention is impossible by design.
- **Validates incoming RS485 data** — received values are checked against physical bounds (5-50 C, 1-99% RH). Out-of-range values from corrupt or truncated packets are rejected and logged to Serial; no CSV write occurs.
- **Controls the LED indicators** based on sensor status -- solid green when all sensors are within the threshold margin, slow red blink when one or more sensors are out of range, and fast red blink on a sensor read failure.
- **Logs sensor data to SD card** every 5 minutes on strict 5-minute clock boundaries, writing timestamped rows to daily files. A midnight janitor automatically deletes files older than 180 days.
- **Serves a web dashboard per room** -- Aging Room at `/`, Skit Room at `/skit`, Camera Room at `/camera` -- each with interactive Chart.js charts, live sensor status bars, system status panels, and threshold adjustment controls. All dashboards auto-refresh on staggered timers (charts every ~307 seconds, status every 29 seconds).
- **Provides a hidden admin page** at `/admin` for SD card file management. Lists all files with sizes, shows total SD used space, allows individual file deletion, and provides a Delete All option for date-prefixed data files (protected active files are excluded).
- **Syncs time via NTP** at startup and twice daily (midnight and noon).

---

## System Architecture

```
+-----------------------------+
|   Arduino Mega (Aging Room) |
|   - 4x DHT22 sensors        |
|   - Ethernet shield          |
|   - SD card                  |
|   - LCD 20x4                 |
|   - Serial1 TX=18, RX=19     |
|   - RS485 DE+RE on pin 34    |
+-----------------------------+
           |
        RS485 bus (twisted pair)
           |
   +-------+-------+
   |               |
+--+----------+  +-+------------+
| Skit Room   |  | Camera Room  |
| UNO R3      |  | UNO R3       |
| 1x DHT22    |  | 1x DHT22     |
| Poll: 5 min |  | Poll: 5 min  |
| prefix SKIT:|  | prefix CAM:  |
+-------------+  +--------------+
```

---

## RS485 Network

### Overview

The Mega controls the RS485 bus using a poll/respond architecture. Every 5 minutes on strict clock boundaries (:00, :05, :10 ... :55), the Mega sends a poll request to each node sequentially. Only the addressed UNO responds. Bus contention is impossible by design.

On boot, an immediate poll fires as soon as NTP time is valid, providing fresh readings without waiting for the next 5-minute boundary.

**Mega:** sends `GET:SKIT\n`, waits up to 2 seconds for reply, then sends `GET:CAM\n`.
**Skit Room UNO:** listens for `GET:SKIT\n`, responds with `SKIT:21.5,45.2\n`, returns to listen mode.
**Camera Room UNO:** listens for `GET:CAM\n`, responds with `CAM:21.5,45.2\n`, returns to listen mode.

### Baud Rate

All RS485 devices use **9600 baud**. SoftwareSerial on UNO is unreliable above ~57600; 9600 is stable for the short DHT22 packets used here.

### Wiring

**Skit Room / Camera Room UNO MAX485:**

| MAX485 Pin | Connection         |
|------------|--------------------|
| VCC        | UNO 5V             |
| GND        | UNO GND            |
| DI         | UNO pin 11 (SoftwareSerial TX) |
| RO         | UNO pin 8 (SoftwareSerial RX)         |
| DE         | UNO pin 10 (firmware controlled) |
| RE         | UNO pin 10 (tied to DE) |
| A          | RS485 bus A        |
| B          | RS485 bus B        |

**CRITICAL:** DE and RE must be wired to a firmware-controlled pin (pin 10), NOT to VCC (5V). Hardwiring DE to VCC with multiple transmitters on the same bus causes both UNOs to drive simultaneously, resulting in bus contention and garbage data.

**Mega MAX485:**

| MAX485 Pin | Connection         |
|------------|--------------------|
| VCC        | Mega 5V            |
| GND        | Mega GND           |
| DI         | Mega pin 18 (Serial1 TX) |
| RO         | Mega pin 19 (Serial1 RX) |
| DE         | Mega pin 34 (firmware controlled) |
| RE         | Mega pin 34 (tied to DE) |
| A          | RS485 bus A        |
| B          | RS485 bus B        |

### Bus Topology

```
UNO (Skit) MAX485      UNO (Camera) MAX485     Mega MAX485
     A ----+------------------+------------------+---- A
     B ----+------------------+------------------+---- B
           |                  |                  |
         120 ohm            (none)            120 ohm
         termination                          termination
```

120-ohm termination resistors at each end of the bus.

---

## Requirements

### Hardware (Aging Room / Mega)
- Arduino Mega 2560
- Wiznet W5500 Ethernet shield (or equivalent)
- MicroSD card (FAT32 formatted, 32 GB or smaller)
- 4x DHT22 temperature/humidity sensors
- MAX485 TTL RS485 transceiver module
- 20x4 I2C LCD display
- 2x LEDs (red, green)
- 1x push button

### Hardware (Skit Room / Camera Room UNO nodes)
- 2x Arduino UNO R3
- 2x DHT22 temperature/humidity sensors
- 2x MAX485 TTL RS485 transceiver modules

### Libraries (Arduino IDE)
- DHT sensor library (Adafruit)
- Adafruit Unified Sensor
- Ethernet (built-in)
- SD (built-in)
- Wire (built-in)
- LiquidCrystal_I2C
- ArduinoJson (optional, not used in current build)

---

## File Overview

### Aging Room (Mega)

| File              | Purpose |
|-------------------|---------|
| `Aging_Room.ino`  | Main sketch, setup/loop, HTTP router |
| `config.h`        | All constants and pin definitions |
| `network.h/cpp`   | Ethernet, connection tracking, NTP, DST, time functions |
| `sensors.h/cpp`   | DHT22 reading, RS485 parsing, LED control, button |
| `display.h/cpp`   | LCD initialization and display updates |
| `storage.h/cpp`   | CSV logging and all HTML/endpoint generation |
| `README.md`       | This file |
| `CHANGELOG.md`    | Version history and change log |

### Skit Room / Camera Room (UNO Nodes)

| File              | Purpose |
|-------------------|---------|
| `Skit_Room/Skit_Room.ino`     | UNO sketch -- listens for Mega poll, responds with DHT22 reading |
| `Camera_Room/Camera_Room.ino` | UNO sketch -- listens for Mega poll, responds with DHT22 reading |

---

## Installation

### Step 1 -- Clone the repository

```bash
git clone https://gitlab.com/seegrid/quality/Aging_Room
cd Aging_Room
```

### Step 2 -- Install required libraries

Open Arduino IDE, go to **Sketch -> Include Library -> Manage Libraries**, and install each library listed in the Requirements section.

### Step 3 -- Configure the network

Edit `Aging_Room.ino` if you need to change the static IP fallback:

```cpp
IPAddress ip(192, 168, 48, 20);
IPAddress gateway(192, 168, 48, 1);
IPAddress subnet(255, 255, 255, 0);
```

### Step 4 -- Set the authentication password

See [Setting Your Authentication Password](#setting-your-authentication-password). **This step is required before deployment.**

### Step 5 -- Upload to Arduino Mega

1. Open `Aging_Room.ino` in Arduino IDE.
2. Select **Tools -> Board -> Arduino Mega 2560**.
3. Select the correct COM port.
4. Click **Upload**.

### Step 6 -- Upload UNO sketches

1. Open `Skit_Room/Skit_Room.ino`.
2. Select **Tools -> Board -> Arduino UNO**.
3. Upload to the Skit Room UNO.
4. Repeat with `Camera_Room/Camera_Room.ino` for the Camera Room UNO.

---

## Setting Your Authentication Password

The system uses **salted SHA256 hashing** for web authentication. You must generate a hash before deploying -- the placeholder in `config.h` will not work.

**Default username:** `Seegrid`
**Default password:** Not set -- you must configure one.

### Step 1 -- Choose your salt and password

```
Salt:     SeegridPittsburgh2026    (example -- make yours unique)
Password: MySecure!Pass2026        (example -- use your own)
```

### Step 2 -- Concatenate salt + password (salt first, no separator)

Combine them with no spaces or separators, salt first:

```
SeegridPittsburgh2026MySecure!Pass2026
```

### Step 3 -- Generate the SHA256 hash

Go to https://emn178.github.io/online-tools/sha256.html, paste the combined string from Step 2, and copy the resulting 64-character hash.

### Step 4 -- Update config.h

```cpp
const char AUTH_SALT[]     = "SeegridPittsburgh2026";
const char PASSWORD_HASH[] = "<your-64-char-hash-here>";
```

### Step 5 -- Save and upload

Save `config.h` and re-upload the sketch to your Arduino.

---

### Example Walkthrough

```
Step 1: Choose values
  Salt:     MyCompanySalt2026
  Password: SecurePass123!

Step 2: Combine (no spaces)
  Combined: MyCompanySalt2026SecurePass123!

Step 3: Generate hash at website
  Input:  MyCompanySalt2026SecurePass123!
  Output: 7a8f9b2c3d4e5f6a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4

Step 4: Put in config.h
  const char AUTH_SALT[]     = "MyCompanySalt2026";
  const char PASSWORD_HASH[] = "7a8f9b2c3d4e5f6a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4";
```

---

### Password Security Guidelines

**DO:**
- Change the default salt to something unique per installation
- Use a strong password (12+ characters, mixed case, numbers, symbols)
- Use a different salt for each deployed unit
- Store your salt and password in a secure password manager

**DON'T:**
- Use the default salt in production
- Commit your actual password or salt to a public Git repository
- Add spaces or separators when combining salt and password
- Reuse salts across multiple deployments

---

### Regenerating Your Password

If you need to change the password after deployment:

1. Keep the same salt or choose a new one.
2. Combine the new salt and new password.
3. Generate a new SHA256 hash at the website.
4. Update both values in `config.h`.
5. Re-upload the sketch to the Arduino.

---

## Memory Architecture and Data Persistence

The Arduino Mega uses four distinct memory types. Understanding where data lives explains how the system behaves after a power outage.

### Flash Memory -- Where Credentials Are Stored

Flash stores the compiled program code and all constants, including your authentication salt and password hash.

| Property | Value |
|----------|-------|
| Size | 256 KB |
| Survives power loss | **Yes** |
| Write method | Arduino IDE upload only |
| Runtime modification | Not possible |

Because credentials are in Flash, they cannot be changed by a running bug, network attack, or power event. Changing them requires uploading a new sketch via USB.

---

### EEPROM -- Where Temperature Thresholds Are Stored

EEPROM stores the user-configured temperature thresholds for all five rooms. It is not used for credentials because it is too easily readable and writable at runtime.

| Property | Value |
|----------|-------|
| Size | 4 KB |
| Survives power loss | **Yes** |
| Write method | Button interface or web dashboard at runtime |

```cpp
// sensors.cpp -- Reading threshold on startup
void initSensors() {
  EEPROM.get(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  if (tempThreshold < MIN_THRESHOLD || tempThreshold > MAX_THRESHOLD) {
    tempThreshold = DEFAULT_TEMP_THRESHOLD;
  }
}

// sensors.cpp -- Saving threshold when user adjusts via button or web
void saveThreshold() {
  EEPROM.put(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
}
```

---

### RAM -- Volatile Working Memory

RAM holds all runtime variables (sensor readings, connection state, HTTP buffers) and is lost immediately on power loss. The system re-initializes everything from Flash and EEPROM at each boot. All string literals are wrapped in the `F()` macro to store them in Flash instead of SRAM.

| Property | Value |
|----------|-------|
| Size | 8 KB |
| Survives power loss | **No** |

**RAM health thresholds** (reported live in the System Status panel on each dashboard via `freeMemory()`):

| Level | Status |
|-------|--------|
| Above 2 KB | Comfortable |
| 1.5--2 KB | Yellow zone -- monitor |
| Below 1.5 KB | Red zone -- apply F() macro pass |
| Below 1 KB | Crash/watchdog reset risk |
| Below 512 bytes | Imminent crash |

---

### SD Card -- Historical Sensor Data

The SD card stores date-prefixed daily CSV files for all three rooms. Data accumulates continuously and survives power outages. The last write before a power loss may be incomplete but all prior records are unaffected.

| Property | Value |
|----------|-------|
| Size | User-supplied (FAT32, 32 GB or smaller) |
| Survives power loss | **Yes** |
| Auto-purge | Files older than 180 days deleted at midnight |

---

### Memory Persistence Summary

| Memory Type | Size | Survives Power Loss | What's Stored Here |
|-------------|------|---------------------|--------------------|
| **Flash** | 256 KB | **Yes** | Program code, credentials |
| **EEPROM** | 4 KB | **Yes** | Temperature thresholds (all 5 rooms) |
| **RAM** | 8 KB | **No** | Runtime state, buffers |
| **SD Card** | User | **Yes** | Historical CSV sensor data |

---

### Power Outage Recovery Sequence

```
[0s]    Power on
[1s]    Flash loads program code and credentials
[2s]    EEPROM reads temperature thresholds for all rooms
[3s]    Display shows boot sequence
[5s]    Sensors initialize
[10s]   Network attempts DHCP
[15s]   Device IP address displayed on LCD
[20s]   NTP time sync begins
[25s]   RS485 immediate boot poll fires (fresh Skit + Camera readings)
[30s]   System fully operational
```

No user action is required after a power outage. Credentials, threshold settings, and all historical data are automatically available.

---

## Web Interface URL Reference

| URL | Method | Description | Auth |
|-----|--------|-------------|------|
| `/` | GET | Aging Room dashboard | Required |
| `/status` | GET | Aging Room sensor JSON | Required |
| `/sysinfo` | GET | Aging Room system info JSON | Required |
| `/threshold` | GET | Aging Room threshold JSON | Required |
| `/threshold` | POST | Update Aging Room threshold | Required |
| `/data` | GET | Aging Room CSV data | Required |
| `/skit` | GET | Skit Room dashboard | Required |
| `/skit/status` | GET | Skit Room sensor JSON | Required |
| `/skit/sysinfo` | GET | Skit Room system info JSON | Required |
| `/skit/threshold/temp` | GET/POST | Skit Room temp threshold | Required |
| `/skit/threshold/humid` | GET/POST | Skit Room humid threshold | Required |
| `/camera` | GET | Camera Room dashboard | Required |
| `/camera/status` | GET | Camera Room sensor JSON | Required |
| `/camera/sysinfo` | GET | Camera Room system info JSON | Required |
| `/camera/threshold/temp` | GET/POST | Camera Room temp threshold | Required |
| `/camera/threshold/humid` | GET/POST | Camera Room humid threshold | Required |
| `/admin` | GET | SD file management (hidden) | Required |
| `/admin/delete` | POST | Delete a named file | Required |
| `/admin/delete-all` | POST | Delete all non-protected data files | Required |
| `/eject` | GET | Halt CSV writes, safe SD removal | Required |

---

## Usage

1. Power on the system. The LCD shows boot status and sensor readings.
2. Open a browser and navigate to `http://agingroom00.mach.hq.seegrid.lan` or the static fallback IP `192.168.48.20`.
3. Log in with username `Seegrid` and your configured password.
4. The Aging Room dashboard loads with live sensor readings and historical charts.
5. Use the navigation links to switch between Aging Room, Skit Room, and Camera Room dashboards.
6. All dashboards auto-refresh on staggered timers. Use the **Update Now** button to force an immediate chart refresh.

---

## Adjusting the Temperature Threshold

The temperature threshold controls the alert range. All five rooms have independent thresholds persisted in EEPROM.

**Via physical button (Aging Room only):**
- Short press: increment threshold by 1 C
- Long press (3+ seconds): decrement threshold by 1 C
- The LCD flashes and LEDs blink to confirm the change.

**Via web dashboard:**
- Each room's dashboard has threshold controls in the System Status panel.
- Changes take effect immediately and persist across power cycles.

---

## Security Features

- **Salted SHA256 password hashing** -- passwords are never stored in plaintext. The salt is prepended before hashing.
- **Connection rate limiting** -- max 8 simultaneous tracked connections. Returns HTTP 503 with Retry-After header when exceeded.
- **Hardware watchdog** -- 8-second watchdog timer. If the main loop stalls, the Mega resets automatically.
- **Protected file deletion** -- active data files cannot be deleted via the admin page. The current daily files for all six data streams are protected.

---

## Data Logging

Data is logged every 5 minutes to date-prefixed daily CSV files on the SD card.

| Room | Temp file | Humid file |
|------|-----------|------------|
| Aging Room | `YYMMDD_T.csv` | `YYMMDD_H.csv` |
| Skit Room | `YYMMDDST.csv` | `YYMMDDSH.csv` |
| Camera Room | `YYMMDDCT.csv` | `YYMMDDCH.csv` |

Each row: `epoch_timestamp,value`

Multi-day chart views stitch multiple daily files together. Data older than 180 days is automatically purged on boot.

---

## Configuration Constants

Key constants in `config.h`:

| Constant | Default | Description |
|----------|---------|-------------|
| `RS485_BAUD` | 9600 | RS485 baud rate (must match UNO sketches) |
| `RS485_TIMEOUT_MS` | 600000 | RS485 receive timeout (10 min) before ERR logged |
| `CSV_WRITE_INTERVAL_MS` | 300000 | CSV write interval (5 min) |
| `CONNECTION_TRACKING_SIZE` | 8 | Max simultaneous tracked connections |
| `AUTH_SALT` | (set in config.h) | Salt prepended to password before hashing |
| `PASSWORD_HASH` | (set in config.h) | SHA256 hash of salt+password |

---

## Troubleshooting

### Skit or Camera Room shows no data or dashes
- Check RS485 wiring: A to A and B to B between all three MAX485 modules.
- Confirm Mega MAX485 DE+RE is wired to **pin 34** (not GND).
- Confirm each UNO MAX485 RO is wired to **UNO pin 8**.
- Confirm DE+RE on each UNO MAX485 is wired to **pin 10**.
- Open Mega Serial Monitor -- it should show `Skit: XX.XC` and `Camera: XX.XC` every 5 minutes.
- If Serial Monitor shows `RS485 no reply: GET:SKIT` the UNO is not responding -- check wiring and confirm UNO sketch is flashed.
- The `Last Receive` timestamp in the Skit/Camera System Status panel shows when data was last received.

### Only one of Skit or Camera shows up on the Mega
- Almost always caused by DE hardwired HIGH on both UNOs -- both drivers permanently active and fighting each other on the bus.
- Confirm DE+RE on each UNO is connected to **pin 10** (firmware-controlled), not to VCC (5V).
- Measure DE pin on each MAX485 with a multimeter -- should read ~0V at idle and only pulse HIGH briefly during transmission.

### Skit or Camera CSV contains a physically impossible value (e.g. 0.6 C)
- Caused by a corrupt or truncated RS485 packet logged before v1.21.
- On v1.21+, out-of-range values (outside 5-50 C or 1-99% RH) are rejected at parse time and a Serial message is printed.
- The stale bad row already in the CSV can be deleted via the admin page (delete the specific date file) or ignored -- it will appear as a spike on that day's chart only.

### RS485 modules not working -- direction issues
- Confirm you are using a **MAX485 TTL module**, NOT an RS232-to-RS485 converter.
- RS232-to-RS485 converters use +/-12V logic levels and will not work with Arduino.
- Ensure DE and RE pins are tied together and connected to pin 10 on each UNO.
- On the Mega side, DE+RE must be wired to **pin 34** (firmware controlled) -- not GND and not 5V.

### Dashboard shows "SYSTEM OFFLINE" banner
- The Arduino is unreachable. Dashboard retries automatically every 4 seconds.
- Check Ethernet cable and power.
- If device rebooted, wait ~30 seconds for boot to complete.

### Chart not rendering
- Confirm browser has internet access to `cdn.jsdelivr.net` for Chart.js.
- Click "Update Now" to force a data refresh.
- If device has been running less than 5 minutes, no CSV data exists yet.

### DHCP failed / cannot reach web interface
- Device falls back to static IP `192.168.48.20`.
- Try hostname: `agingroom00.mach.hq.seegrid.lan`

### Time is off by one hour
- Confirm you are running v1.10 or later which includes the DST month index fix.
- Serial Monitor shows `DST Active: Yes (EDT)` or `DST Active: No (EST)`.

### SD card initialization failed
- Confirm SD card is formatted FAT32.
- Cards larger than 32 GB may need reformatting -- Windows defaults to exFAT which is not compatible.

### Cannot delete a file on the admin page
- Active data files are protected from deletion via the admin page.
- Use `/eject` + manual SD removal for full access to delete any file outside the admin page.

### RAM is low (below 1.5 KB shown in System Status)
- All string literals should be wrapped in `F()` macro -- verify no bare string literals remain in `network.cpp` or `storage.cpp`.
- Reduce `CONNECTION_TRACKING_SIZE` in `config.h` from 8 to 4 if RAM remains critically low.

---

## Version History

See [CHANGELOG.md](CHANGELOG.md) for full version history.

---

## Acknowledgments

Built for Seegrid aging room environmental monitoring. Uses Chart.js for web visualization, chartjs-plugin-zoom and hammerjs for chart interaction, and chartjs-adapter-date-fns for time axis formatting. NTP implementation based on Arduino examples. Security features implement OWASP best practices for password storage.
