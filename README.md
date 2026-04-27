# Seegrid Aging Room Environmental Monitoring System

An Arduino Mega-based multi-room temperature and humidity monitoring system for industrial aging room environments. Reads four DHT22 sensors on the Aging Room, receives wireless RS485 sensor data from Skit Room and Camera Room Nano nodes, logs all data to an SD card, and serves interactive web dashboards per room with real-time alerts.

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
- **Receives RS485 serial data** from Skit Room and Camera Room Arduino Nano nodes every 6 minutes. Each Nano reads a DHT22 sensor and transmits a packet (`SKIT:21.5,45.2\n` or `CAM:21.5,45.2\n`) over RS485. The Mega parses and stores the values.
- **Controls the LED indicators** based on sensor status — solid green when all sensors are within the threshold margin, slow red blink when one or more sensors are out of range, and fast red blink on a sensor read failure.
- **Logs sensor data to SD card** every 5 minutes on strict 5-minute clock boundaries, writing timestamped rows to daily files. A midnight janitor automatically deletes files older than 180 days. Data survives power outages and can be downloaded directly from the web interface.
- **Serves a web dashboard per room** — Aging Room at `/`, Skit Room at `/skit`, Camera Room at `/camera` — each with interactive Chart.js charts, live sensor status bars, system status panels, and threshold adjustment controls.
- **Provides a hidden admin page** at `/admin` for SD card file management — listing all files with sizes, downloading individual files, and deleting files with safety guards on active data files.
- **Syncs time via NTP** at startup and twice daily (midnight and noon). The system first contacts the internal NTP server `192.168.80.8` and falls back to the public NTP pool `pool.ntp.org` if unavailable. DST transitions occur correctly at 2:00 AM on the 2nd Sunday of March (EDT) and 1st Sunday of November (EST).
- **Runs a hardware watchdog** armed at 8 seconds. If the system stalls for any reason — network hang, SD deadlock, infinite loop — the Arduino reboots automatically. Each reboot is timestamped and logged to `EVENTS.txt` on the SD card.
- **Tracks network connections** per IP address. A maximum of 8 simultaneous global connections and 3 per IP are enforced; idle connections are released after 5 minutes.
- **Persists temperature thresholds** to EEPROM — one each for Aging Room, Skit Room temp, Skit Room humidity, Camera Room temp, and Camera Room humidity — so all user-configured values survive power outages.

---

## System Architecture

```
                    ┌─────────────────────────────┐
                    │     Arduino Mega 2560        │
                    │                              │
                    │  • 4× DHT22 sensors (A–D)   │
                    │  • Ethernet W5100/W5500      │
                    │  • SD Card (logging)         │
                    │  • LCD 20×4 I2C              │
                    │  • Serial1 (pin 19) ←── RS485 bus  │
                    └──────────────┬───────────────┘
                                   │ RS485 Bus (A/B twisted pair)
                    ┌──────────────┴───────────────┐
                    │                              │
          ┌─────────┴──────┐            ┌──────────┴──────┐
          │  Arduino Nano  │            │  Arduino Nano   │
          │  Skit Room     │            │  Camera Room    │
          │  • 1× DHT22    │            │  • 1× DHT22     │
          │  • MAX485 TX   │            │  • MAX485 TX    │
          │  Tx every 6min │            │  Tx every 6min  │
          └────────────────┘            └─────────────────┘
```

---

## RS485 Network

### Overview

The Skit Room and Camera Room each use an **Arduino Nano** with a **MAX485 TTL-to-RS485 module** to transmit sensor data to the Mega every 6 minutes. The Mega listens on `Serial1` (hardware UART, pin 19 RX) and parses incoming packets.

### Packet Format

| Room        | Packet Format          | Example              |
|-------------|------------------------|----------------------|
| Skit Room   | `SKIT:temp,humid\n`    | `SKIT:21.5,45.2\n`   |
| Camera Room | `CAM:temp,humid\n`     | `CAM:20.3,48.4\n`    |

- Units are raw floats — no `C` or `%` suffix in the packet (formatting is applied by the Mega on display/storage)
- On sensor failure the Nano sends `SKIT:ERR,ERR\n` so the Mega knows the sensor failed vs. a lost packet

### RS485 Module Selection

> **Important:** You must use a **MAX485 TTL module** — NOT an RS232-to-RS485 converter.

| Module Type | Correct for Arduino? | Notes |
|-------------|---------------------|-------|
| **MAX485 TTL breakout** | ✅ Yes | 5V logic, DI/DE/RE/RO pins, ~$1–2 each |
| RS232 to RS485 converter | ❌ No | Uses ±12V RS232 levels — will damage Arduino pins |
| USB to RS485 adapter | ❌ No | For PC use only |

Search Amazon for: **"MAX485 module Arduino"** — look for the small green board with DI, DE, RE, RO, VCC, GND, A, B labeled.

### DE/RE Pin Wiring

The MAX485 chip uses two direction control pins:

| Pin | Name | Function |
|-----|------|----------|
| DI | Driver Input | Data to transmit — connect to Arduino TX |
| DE | Driver Enable | HIGH = transmit mode enabled |
| RE | Receiver Enable | LOW = receive mode enabled |
| RO | Receiver Output | Received data — connect to Arduino RX |

Since DE and RE are always set opposite each other, **tie DE and RE together to a single Arduino digital pin**:
- Pin HIGH → transmit mode
- Pin LOW → receive mode

### Current Wiring Status (Temporary)

While proper MAX485 modules are on order, the Skit Room sensor is wired **directly to pin 19 (Serial1 RX)** on the Mega as a temporary bypass. Data is coming through correctly. The Camera Room is not yet connected.

Once MAX485 modules arrive:
1. Wire Nano TX → MAX485 DI, Nano DE/RE pin → MAX485 DE+RE tied together, MAX485 A/B → bus
2. Wire Mega Serial1 RX (pin 19) → MAX485 RO, Mega DE/RE pin → MAX485 DE+RE, same A/B bus
3. No firmware changes required — the code already handles DE/RE switching

### Transmit-Only Nodes (Skit/Camera Nanos)

Because the Skit and Camera Nanos **only ever transmit** and never need to receive, the DE/RE direction control is less critical on the transmitter side. A module with DE hardwired HIGH will work fine on the Nano transmit side. The Mega receive side needs RE permanently LOW or proper DE/RE control.

---

## Requirements

### Hardware

- Arduino Mega 2560 (or compatible)
- Ethernet Shield W5100 or W5500
- 4× DHT22 temperature/humidity sensors (Aging Room)
- 2× Arduino Nano (Skit Room, Camera Room)
- 3× MAX485 TTL-to-RS485 modules (1 per Nano TX, 1 on Mega RX)
- 20×4 I2C LCD display (I2C address `0x27`)
- SD card module
- Red and green LEDs
- Push button
- MicroSD card formatted FAT32
- RS485 twisted pair cable between rooms

### Software

- Arduino IDE
- The following libraries (install via Library Manager):

```
DHT sensor library by Adafruit
Adafruit Unified Sensor
LiquidCrystal I2C
Ethernet (built-in)
SD (built-in)
Crypto by Rhys Weatherley
arduino-base64 by Densaugeo
SoftwareSerial (built-in, for Nano RS485 sketches)
```

### Pin Configuration — Mega

| Component            | Pin | Notes        |
|----------------------|-----|--------------| 
| DHT22 Sensor A       | 40  | Digital      |
| DHT22 Sensor B       | 41  | Digital      |
| DHT22 Sensor C       | 30  | Digital      |
| DHT22 Sensor D       | 31  | Digital      |
| Green LED            | 47  | Digital      |
| Red LED              | 46  | Digital      |
| Ethernet CS          | 10  | SPI          |
| Button               | 50  | INPUT_PULLUP |
| SD Card CS           | 4   | SPI          |
| LCD                  | I2C | SDA/SCL      |
| RS485 RX (Serial1)   | 19  | Hardware UART |
| RS485 DE/RE          | TBD | Digital      |

### Pin Configuration — Nano (Skit Room / Camera Room)

| Component     | Pin | Notes                  |
|---------------|-----|------------------------|
| DHT22 Data    | 4   | Digital                |
| MAX485 DI     | 7   | SoftwareSerial TX      |
| MAX485 RO     | 8   | SoftwareSerial RX      |
| MAX485 DE+RE  | TBD | Digital (tied together)|

---

## File Overview

### Mega (Main Controller)

| File              | Purpose |
|-------------------|---------| 
| `Aging_Room.ino`  | Main program — `setup()`, `loop()`, HTTP router |
| `config.h`        | All configuration constants and includes |
| `auth.h`          | Authentication function declarations |
| `auth.cpp`        | Salted SHA256 password validation logic |
| `network.h`       | Network and NTP declarations |
| `network.cpp`     | Connection tracking, NTP, DST, and time functions |
| `sensors.h`       | Sensor and LED declarations |
| `sensors.cpp`     | DHT22 reading, RS485 parsing, LED control, button |
| `display.h`       | LCD display declarations |
| `display.cpp`     | LCD initialization and display updates |
| `storage.h`       | SD card and web serving declarations |
| `storage.cpp`     | CSV logging and all HTML/endpoint generation |
| `README.md`       | This file |
| `CHANGELOG.md`    | Version history and change log |

### Skit Room / Camera Room (Nano Nodes)

| File              | Purpose |
|-------------------|---------|
| `Skit_Room/Skit_Room.ino`     | Nano sketch — reads DHT22, transmits RS485 every 6 min |
| `Camera_Room/Camera_Room.ino` | Nano sketch — reads DHT22, transmits RS485 every 6 min |

---

## Installation

### Step 1 — Clone the repository

```bash
git clone https://gitlab.com/seegrid/quality/Aging_Room
cd Aging_Room
```

### Step 2 — Install required libraries

Open Arduino IDE, go to **Sketch → Include Library → Manage Libraries**, and install each library listed in the [Requirements](#requirements) section.

### Step 3 — Configure the network

Edit `Aging_Room.ino` if you need to change the static IP fallback used when DHCP is unavailable:

```cpp
IPAddress ip(192, 168, 48, 20);
IPAddress gateway(192, 168, 48, 1);
IPAddress subnet(255, 255, 255, 0);
```

### Step 4 — Set the authentication password

See [Setting Your Authentication Password](#setting-your-authentication-password) below. **This step is required before deployment.**

### Step 5 — Upload to Arduino Mega

1. Open `Aging_Room.ino` in Arduino IDE.
2. Select **Tools → Board → Arduino Mega 2560**.
3. Select the correct COM port under **Tools → Port**.
4. Click **Upload**.

### Step 6 — Upload Nano sketches

1. Open `Skit_Room/Skit_Room.ino`.
2. Select **Tools → Board → Arduino Nano**.
3. Upload to the Skit Room Nano.
4. Repeat with `Camera_Room/Camera_Room.ino` for the Camera Room Nano.

---

## Setting Your Authentication Password

The system uses **salted SHA256 hashing** for web authentication. You must generate a hash before deploying — the placeholder in `config.h` will not work.

**Default username:** `Seegrid`  
**Default password:** Not set — you must configure one.

### Step 1 — Choose your salt and password

```
Salt:     SeegridPittsburgh2026    (example — make yours unique)
Password: MySecure!Pass2026        (example — use your own)
```

### Step 2 — Concatenate salt and password

Combine them with no spaces or separators, salt first:

```
SeegridPittsburgh2026MySecure!Pass2026
```

### Step 3 — Generate the SHA256 hash

Go to https://emn178.github.io/online-tools/sha256.html, paste the combined string from Step 2, and copy the resulting 64-character hash.

### Step 4 — Update `config.h`

```cpp
#define AUTH_SALT            "SeegridPittsburgh2026"
#define AUTH_PASSWORD_SHA256 "your_copied_hash_here"
```

### Step 5 — Save and upload

Save `config.h` and re-upload the sketch to your Arduino.

---

### Password Security Guidelines

**DO:**
- ✓ Change the default salt to something unique per installation
- ✓ Use a strong password (12+ characters, mixed case, numbers, symbols)
- ✓ Use a different salt for each deployed unit
- ✓ Store your salt and password in a secure password manager

**DON'T:**
- ✗ Use the default salt in production
- ✗ Commit your actual password or salt to Git
- ✗ Add spaces or separators when combining salt and password
- ✗ Reuse salts across multiple deployments

---

## Memory Architecture and Data Persistence

### Flash Memory — Program + Credentials

| Property | Value |
|----------|-------|
| Size | 256 KB |
| Survives power loss | **Yes** |
| Write method | Arduino IDE upload only |

### EEPROM — Threshold Values

| Property | Value |
|----------|-------|
| Size | 4 KB |
| Current usage | 5 thresholds × 4 bytes = 20 bytes |
| Survives power loss | **Yes** |

EEPROM stores: Aging Room temp threshold, Skit Room temp threshold, Skit Room humid threshold, Camera Room temp threshold, Camera Room humid threshold.

### RAM — Volatile Working Memory

| Property | Value |
|----------|-------|
| Size | 8 KB |
| Survives power loss | **No** |
| Typical free RAM | ~2.0 KB at runtime |

All HTML strings use `F()` macro to store in Flash rather than RAM. RAM is monitored live in the System Status panel on each dashboard.

### SD Card — Historical Sensor Data

| Property | Value |
|----------|-------|
| Survives power loss | **Yes** |
| Estimated growth | ~14 KB/day per room · ~15 MB/year total |
| Retention | 180 days rolling |

### Memory Persistence Summary

| Memory Type | Size   | Survives Power Loss | What's Stored |
|-------------|--------|---------------------|---------------|
| **Flash**   | 256 KB | **Yes**             | Program + auth credentials |
| **EEPROM**  | 4 KB   | **Yes**             | 5 threshold values |
| **RAM**     | 8 KB   | **No**              | Runtime variables |
| **SD Card** | User   | **Yes**             | All historical CSV data |

---

## Web Interface URL Reference

All endpoints require HTTP Basic Auth. Replace `192.168.55.151` with your device's actual IP.

### Full HTML Pages

| URL | Description |
|-----|-------------|
| `http://192.168.55.151/` | Aging Room main dashboard |
| `http://192.168.55.151/skit` | Skit Room dashboard |
| `http://192.168.55.151/camera` | Camera Room dashboard |
| `http://192.168.55.151/admin` | **Hidden** admin SD file manager — no nav link |

### Data / API Endpoints

| URL | Returns | Notes |
|-----|---------|-------|
| `http://192.168.55.151/status` | `A:21.3\|OK\|45.1,B:20.9\|LOW\|44.8,...` | Aging Room sensor states |
| `http://192.168.55.151/sysinfo` | `RAM:2048,UPTIME:0d 5h 58m,SD:OK,...` | System info |
| `http://192.168.55.151/threshold` | `21.0` | Aging Room temp threshold |
| `http://192.168.55.151/events` | Plain text | Watchdog reboot log |
| `http://192.168.55.151/temp.csv` | CSV | Aging Room temperature (last 7 days) |
| `http://192.168.55.151/humid.csv` | CSV | Aging Room humidity (last 7 days) |
| `http://192.168.55.151/skit/status` | `TEMP:20.7\|OK,HUMID:50.2\|OK` | Skit Room sensor state |
| `http://192.168.55.151/skit/sysinfo` | `RAM:...,LASTRECEIVE:...` | Skit Room system info |
| `http://192.168.55.151/skit/threshold/temp` | `22.0` | Skit Room temp threshold |
| `http://192.168.55.151/skit/threshold/humid` | `50.0` | Skit Room humid threshold |
| `http://192.168.55.151/skit/temp.csv` | CSV | Skit Room temperature (last 7 days) |
| `http://192.168.55.151/skit/humid.csv` | CSV | Skit Room humidity (last 7 days) |
| `http://192.168.55.151/camera/status` | `TEMP:20.3\|OK,HUMID:48.4\|OK` | Camera Room sensor state |
| `http://192.168.55.151/camera/sysinfo` | `RAM:...,LASTRECEIVE:...` | Camera Room system info |
| `http://192.168.55.151/camera/threshold/temp` | `22.0` | Camera Room temp threshold |
| `http://192.168.55.151/camera/threshold/humid` | `50.0` | Camera Room humid threshold |
| `http://192.168.55.151/camera/temp.csv` | CSV | Camera Room temperature (last 7 days) |
| `http://192.168.55.151/camera/humid.csv` | CSV | Camera Room humidity (last 7 days) |
| `http://192.168.55.151/archive?file=FILENAME` | File download | Download any file by name from SD |

### Action Endpoints

| URL | Action | Notes |
|-----|--------|-------|
| `http://192.168.55.151/clear-events` | Deletes `EVENTS.txt` | Clears watchdog alert log |
| `http://192.168.55.151/cleanup` | Deletes `temp.csv` and `humid.csv` | Legacy file cleanup |
| `http://192.168.55.151/eject` | Safely unmounts SD, halts system | Must reboot to resume logging |
| `http://192.168.55.151/admin/delete?file=FILENAME` | Deletes named file from SD | Active data files are blocked |

### Hostname Access

If DNS is configured on your network:

```
http://agingroom00.mach.hq.seegrid.lan/
http://agingroom00.mach.hq.seegrid.lan/skit
http://agingroom00.mach.hq.seegrid.lan/camera
http://agingroom00.mach.hq.seegrid.lan/admin
```

### Static IP Fallback

If DHCP fails, the device falls back to:

```
http://192.168.48.20/
```

---

## Usage

### Finding the Device IP Address

After boot, the device displays its IP address on the LCD for 10 seconds and on the Serial Monitor (115200 baud). The device attempts DHCP first and falls back to the static IP `192.168.48.20` if DHCP fails.

### Accessing the Web Interface

1. Navigate to `http://[DEVICE_IP]` in your browser.
2. Enter credentials when prompted:
   - **Username:** `Seegrid`
   - **Password:** Your configured password

### Web Dashboard Features

**Navigation Bar**  
Every page has buttons to switch between Aging Room, Skit Room, and Camera Room dashboards. The Admin page has no nav link — access it directly at `/admin`.

**Sensor Status Bar**  
Shows live sensor readings with color-coded status. Updates every 29 seconds. Displays temperature on the Temperature tab, humidity on the Humidity tab.

| Dot Color | Meaning |
|-----------|---------| 
| Identity color | Sensor within threshold range |
| Yellow | Sensor outside threshold — shows ↓ LOW or ↑ HIGH |
| Red (blinking) | Sensor error |

**Sensor Identity Colors (Aging Room)**

| Sensor | Color | Hex |
|--------|-------|-----|
| A | Blue | `#0072B2` |
| B | Yellow/Amber | `#E69F00` |
| C | Pink | `#CC79A7` |
| D | Light Blue | `#56B4E9` |

**Chart Tabs**
- Temperature — sensor readings with threshold lines
- Humidity — humidity trends
- Archive Data — date range picker to download combined CSV up to 6 months

**System Status Panel**  
Shows RAM, uptime, SD status, last write/receive time, NTP sync time.

**Threshold Adjustment (Skit/Camera pages)**  
Up/Down buttons adjust temp and humid thresholds live. Values are saved to EEPROM immediately.

**Admin Page (`/admin`)**  
Dark-themed hidden page listing all SD card files with sizes. Each file has a Download button (reuses `/archive` endpoint) and a Delete button with confirmation dialog. Active data files (`temp.csv`, `humid.csv`, `SK_T.csv`, etc.) are protected from deletion. Auto-refreshes after a delete.

### Safe SD Card Removal

1. Click **"Prepare SD for Removal / Halt"** in the System Status panel.
2. Confirm the dialog.
3. Wait for the LCD to display `SD UNMOUNTED / SAFE TO UNPLUG`.
4. Remove the SD card.
5. **You must reboot the Arduino to resume logging.**

---

## Adjusting the Temperature Threshold

### Aging Room — Physical Button

1. Press and hold the button for **5 seconds**.
2. Both LEDs alternate at 250 ms to confirm adjustment mode.
3. Green LED flashes 10 times to confirm entry.
4. Keep holding — threshold increases by 1°C every 2 seconds, cycling −40°C to 80°C.
5. Release the button at the desired value to save.
6. Red LED flashes 10 times, then both flash 20 times to confirm save.
7. The web dashboard threshold lines update automatically within 37 seconds.

### Skit Room / Camera Room — Web Interface

Use the Up/Down arrow buttons in the **Threshold Adjustment** panel on the `/skit` or `/camera` dashboard page. Changes take effect immediately and are saved to EEPROM.

---

## Security Features

### Salted SHA256 Authentication

All web endpoints require authentication. Passwords are stored as `SHA256(SALT + PASSWORD)` in Flash — never in plaintext, never modifiable at runtime.

### IP-Based Connection Limiting

- **Global limit:** 8 simultaneous connections
- **Per-IP limit:** 3 simultaneous connections
- **Idle timeout:** 5 minutes
- **Cleanup interval:** Every 30 seconds

### Hardware Watchdog

8-second watchdog provides automatic crash recovery. All reboots logged to `EVENTS.txt` with timestamps.

---

## Data Logging

| Property | Value |
|----------|-------|
| Interval | Every 5 minutes, time-aligned to clock boundaries |
| Aging Room files | `YYMMDD_T.csv`, `YYMMDD_H.csv` |
| Skit Room files | `YYMMDDST.csv`, `YYMMDDSH.csv` |
| Camera Room files | `YYMMDDCT.csv`, `YYMMDDCH.csv` |
| Watchdog log | `EVENTS.txt` |
| Retention | 180 days rolling — midnight janitor deletes old files |
| Time sync | NTP at boot, midnight, and noon |
| NTP primary | `192.168.80.8` |
| NTP fallback | `pool.ntp.org` |
| Timezone | Eastern Time with automatic DST |

---

## Configuration Constants

Edit `config.h` to change these parameters:

```cpp
#define MIN_THRESHOLD          -40      // Minimum adjustable threshold (°C)
#define MAX_THRESHOLD          80       // Maximum adjustable threshold (°C)
#define DEFAULT_TEMP_THRESHOLD 42       // Default threshold (°C)
#define THRESHOLD_MARGIN       5.0      // Alert margin (±°C)
#define MAX_GLOBAL_CONNECTIONS 8        // Total connection limit
#define MAX_PER_IP_CONNECTIONS 3        // Per-IP connection limit
#define CONNECTION_TIMEOUT     300000   // 5 minutes (ms)
#define SENSOR_READ_INTERVAL   2000     // 2 seconds (ms)
#define CSV_WRITE_INTERVAL     300000   // 5 minutes (ms)
#define NTP_INTERVAL           86400000 // 24 hours (ms)
```

---

## Troubleshooting

### Sensor reading errors / LCD shows "ERR"
- Check DHT22 sensor wiring and confirm 5V power is reaching the sensors.
- Verify the data wire from each M12 connector is landed on the correct Arduino pin.
- For M12 wiring: Pin 2 = Sensor A or C data, Pin 4 = Sensor B or D data, Pin 1 = VCC, Pin 3 = GND.

### Skit Room / Camera Room shows no data or dashes
- Check the RS485 wiring between the Nano and Mega.
- Confirm the Nano sketch is uploaded and running — check its Serial Monitor for `Sent: SKIT:XX.X,XX.X`.
- If using temporary direct wiring (no RS485 modules yet), confirm pin 19 on the Mega is connected to the Nano TX.
- The `Last Receive` timestamp in the Skit/Camera System Status panel shows when data was last received.

### RS485 modules not working — direction issues
- Confirm you are using a **MAX485 TTL module**, NOT an RS232-to-RS485 converter.
- RS232-to-RS485 converters use ±12V logic levels and will not work with Arduino — they are for PC/PLC use only.
- Ensure DE and RE pins are tied together and connected to a digital output pin on the Arduino.
- DE/RE HIGH = transmit mode, DE/RE LOW = receive mode.

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
- Cards larger than 32GB may need reformatting — Windows defaults to exFAT which is not compatible.

### Cannot delete a file on the admin page
- Active data files (`temp.csv`, `humid.csv`, `SK_T.csv`, `SK_H.csv`, `CA_T.csv`, `CA_H.csv`) are protected from deletion on the admin page.
- Use the `/cleanup` endpoint to delete legacy `temp.csv`/`humid.csv`, or use `/eject` + manual SD removal for full access.

---

## Version History

See [CHANGELOG.md](CHANGELOG.md) for full version history.

---

## Acknowledgments

Built for Seegrid aging room environmental monitoring. Uses Chart.js for web visualization, chartjs-plugin-zoom and hammerjs for chart interaction, and chartjs-adapter-date-fns for time axis formatting. NTP implementation based on Arduino examples. Security features implement OWASP best practices for password storage.
