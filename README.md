# Seegrid Aging Room Environmental Monitoring System

An Arduino-based temperature and humidity monitoring system for industrial aging room environments. Reads four DHT22 sensors simultaneously, displays live readings on an LCD, logs data to an SD card, and serves an interactive web dashboard with real-time alerts.

---

## Table of Contents

1. [How It Works](#how-it-works)
2. [Requirements](#requirements)
3. [File Overview](#file-overview)
4. [Installation](#installation)
5. [Setting Your Authentication Password](#setting-your-authentication-password)
6. [Memory Architecture and Data Persistence](#memory-architecture-and-data-persistence)
7. [Usage](#usage)
8. [Adjusting the Temperature Threshold](#adjusting-the-temperature-threshold)
9. [Security Features](#security-features)
10. [Data Logging](#data-logging)
11. [Configuration Constants](#configuration-constants)
12. [Troubleshooting](#troubleshooting)
13. [Version History](#version-history)

---

## How It Works

Once running, the system enters a continuous monitoring loop that:

- **Reads all four DHT22 sensors** every 2 seconds and updates the LCD display. The display rotates between temperature and humidity views automatically.
- **Controls the LED indicators** based on sensor status — solid green when all sensors are within the threshold margin, slow red blink when one or more sensors are out of range, and fast red blink on a sensor read failure.
- **Logs sensor data to SD card** every 5 minutes on strict 5-minute clock boundaries, writing timestamped rows to daily files (`YYMMDD_T.csv` and `YYMMDD_H.csv`). A midnight janitor automatically deletes files older than 180 days to prevent the SD card from filling up. Data survives power outages and can be downloaded directly from the web interface.
- **Serves a web dashboard** with interactive Chart.js charts showing temperature and humidity trends over 1, 3, 5, or 7 days. Charts support scroll-wheel zoom, click-drag pan, and pinch-to-zoom. A sensor status bar shows live temperature in °C and °F per sensor with color-coded threshold state indicators. A System Status panel shows RAM, uptime, SD status, last write time, and last NTP sync. A Watchdog Alerts panel logs any crash-recovery reboots. All endpoints are protected by HTTP Basic Auth using salted SHA256 password hashing.
- **Syncs time via NTP** at startup and every 24 hours thereafter. The system first contacts the internal NTP server `192.168.80.8` and falls back to the public NTP pool `pool.ntp.org` if unavailable. DST transitions occur correctly at 2:00 AM on the 2nd Sunday of March (EDT) and 1st Sunday of November (EST).
- **Runs a hardware watchdog** armed at 8 seconds. If the system stalls for any reason — network hang, SD deadlock, infinite loop — the Arduino reboots automatically. Each reboot is timestamped and logged to `EVENTS.txt` on the SD card.
- **Tracks network connections** per IP address. A maximum of 8 simultaneous global connections and 3 per IP are enforced; idle connections are released after 5 minutes.
- **Persists the temperature threshold** to EEPROM so user-configured values survive power outages. Authentication credentials are stored in Flash and never modified at runtime.

---

## Requirements

### Hardware

- Arduino Mega 2560 (or compatible)
- Ethernet Shield W5100 or W5500
- 4× DHT22 temperature/humidity sensors
- 20×4 I2C LCD display (I2C address `0x27`)
- SD card module
- Red and green LEDs
- Push button
- MicroSD card formatted FAT32

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
```

### Pin Configuration

| Component       | Pin | Notes        |
|-----------------|-----|--------------|
| DHT22 Sensor A  | 40  | Digital      |
| DHT22 Sensor B  | 41  | Digital      |
| DHT22 Sensor C  | 30  | Digital      |
| DHT22 Sensor D  | 31  | Digital      |
| Green LED       | 47  | Digital      |
| Red LED         | 46  | Digital      |
| Ethernet CS     | 10  | SPI          |
| Button          | 50  | INPUT_PULLUP |
| SD Card CS      | 4   | SPI          |
| LCD             | I2C | SDA/SCL      |

---

## File Overview

| File              | Purpose |
|-------------------|---------|
| `Aging_Room.ino`  | Main program — `setup()` and `loop()` |
| `config.h`        | All configuration constants and includes |
| `auth.h`          | Authentication function declarations |
| `auth.cpp`        | Salted SHA256 password validation logic |
| `network.h`       | Network and NTP declarations |
| `network.cpp`     | Connection tracking, NTP, DST, and time functions |
| `sensors.h`       | Sensor and LED declarations |
| `sensors.cpp`     | DHT22 reading, LED control, and button handling |
| `display.h`       | LCD display declarations |
| `display.cpp`     | LCD initialization and display updates |
| `storage.h`       | SD card and web serving declarations |
| `storage.cpp`     | CSV logging and HTML generation |
| `README.md`       | This file |
| `CHANGELOG.md`    | Version history and change log |

> **Note:** Authentication credentials (`AUTH_SALT` and `AUTH_PASSWORD_SHA256`) are defined in `config.h` and compiled into Flash memory. You must set these before uploading. They cannot be changed at runtime.

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

### Step 5 — Upload to Arduino

1. Open `Aging_Room.ino` in Arduino IDE.
2. Select **Tools → Board → Arduino Mega 2560**.
3. Select the correct COM port under **Tools → Port**.
4. Click **Upload**.

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

The Arduino Mega uses four distinct memory types. Understanding where data lives explains how the system behaves after a power outage.

### Flash Memory — Where Credentials Are Stored

| Property | Value |
|----------|-------|
| Size | 256 KB |
| Survives power loss | **Yes** |
| Write method | Arduino IDE upload only |
| Runtime modification | Not possible |

### EEPROM — Where the Temperature Threshold Is Stored

| Property | Value |
|----------|-------|
| Size | 4 KB |
| Current usage | 4 bytes (0.1%) |
| Survives power loss | **Yes** |
| Write method | Button interface at runtime |

### RAM — Volatile Working Memory

| Property | Value |
|----------|-------|
| Size | 8 KB |
| Survives power loss | **No** |

### SD Card — Historical Sensor Data

The SD card stores daily files (`YYMMDD_T.csv` and `YYMMDD_H.csv`) with 180-day rolling retention, and `EVENTS.txt` for watchdog reboot logs.

| Property | Value |
|----------|-------|
| Size | User-supplied (typically 2–32 GB) |
| Survives power loss | **Yes** |
| Retention | 180 days (automated midnight cleanup) |
| Estimated growth | ~14 KB/day · ~5 MB/year |

### Memory Persistence Summary

| Memory Type | Size   | Survives Power Loss | Credentials Stored  |
|-------------|--------|---------------------|---------------------|
| **Flash**   | 256 KB | **Yes**             | **Yes ← auth here** |
| **EEPROM**  | 4 KB   | **Yes**             | No                  |
| **RAM**     | 8 KB   | **No**              | No                  |
| **SD Card** | User   | **Yes**             | No                  |

### Memory Health Indicators

| Memory       | Healthy   | Warning    | Critical |
|--------------|-----------|------------|----------|
| Flash        | < 80%     | 80–95%     | > 95%    |
| EEPROM       | < 50%     | 50–90%     | > 90%    |
| RAM          | < 70%     | 70–85%     | > 85%    |
| SD Card free | > 100 MB  | 10–100 MB  | < 10 MB  |

---

## Usage

### Finding the Device IP Address

After boot, the device displays its IP address and scrolls the DNS hostname on the LCD for 10 seconds, and prints both to the Serial Monitor (115200 baud). The device attempts DHCP first and falls back to the static IP `192.168.48.20` if DHCP fails.

### Accessing the Web Interface

1. Navigate to `http://[DEVICE_IP]` in your browser.
2. Enter credentials when prompted:
   - **Username:** `Seegrid`
   - **Password:** Your configured password

### Web Dashboard Features

**Sensor Status Bar**
Shows live temperature in °C and °F for each sensor with a colored dot indicator. Updates every 30 seconds automatically.

| Dot Color | Meaning |
|-----------|---------|
| Identity color (blue/yellow/pink/light blue) | Sensor within threshold range |
| Yellow | Sensor outside threshold — shows ↓ LOW or ↑ HIGH |
| Red (blinking) | Sensor error — shows ERR |

**Sensor Identity Colors**

| Sensor | Color | Hex |
|--------|-------|-----|
| A | Blue | `#0072B2` |
| B | Yellow/Amber | `#E69F00` |
| C | Pink | `#CC79A7` |
| D | Light Blue | `#56B4E9` |

**Chart Tabs**
- **Temperature tab** — All four sensor readings with Threshold, High Threshold, and Low Threshold lines
- **Humidity tab** — Humidity trends across all four sensors
- **Archive Data tab** — From/To date range picker to download a combined CSV for any period up to 6 months

**Chart Controls**
- Range selector — 1, 3, 5, or 7 day views
- Export PNG — Downloads a chart image
- Update Now — Forces an immediate chart refresh
- Reset Zoom — Returns chart to full time range
- Scroll wheel — Zooms in on x-axis
- Click and drag — Pans the zoomed view
- Pinch (touch) — Zooms on mobile

**System Status Panel** (below charts)
Shows free RAM (color-coded), uptime, SD card status, last CSV write time, and last NTP sync time. Polls every 30 seconds. Also contains the "Prepare SD for Removal / Halt" button.

**Watchdog Alerts Panel** (below System Status)
Shows the 5 most recent entries from `EVENTS.txt`, each timestamped with the date and time of the reboot. Polls every 60 seconds. Contains a "Clear Alerts" button to reset the log.

### Safe SD Card Removal

To safely remove the SD card without corrupting data:

1. Click **"Prepare SD for Removal / Halt"** in the System Status panel.
2. Confirm the dialog.
3. Wait for the LCD to display `SD UNMOUNTED / SAFE TO UNPLUG`.
4. Remove the SD card.
5. **You must reboot the Arduino to resume logging.**

### Chart Resolution by Time Range

| Range   | Data Points Plotted | Resolution     |
|---------|---------------------|----------------|
| 1 day   | Every reading       | Every 5 min    |
| 3 days  | Every 6th reading   | Every 30 min   |
| 5 days  | Every 12th reading  | Every 60 min   |
| 7 days  | Every 12th reading  | Every 60 min   |

---

## Adjusting the Temperature Threshold

The threshold is adjusted using the physical button on the device.

1. Press and hold the button for **5 seconds**.
2. Both LEDs alternate at 250 ms to confirm you are entering adjustment mode.
3. Green LED flashes 10 times to confirm entry.
4. **Keep holding** — the threshold increases by 1°C every 2 seconds. The current value is shown on the LCD.
5. **Release the button** when the desired threshold is displayed to save.
6. Red LED flashes 10 times to confirm the save.
7. Both LEDs flash 20 times as a final confirmation.
8. The display shows the old and new threshold values for 10 seconds.
9. The web dashboard threshold lines update automatically within 30 seconds.

The new threshold is saved to EEPROM and persists through power outages.

### LED Status During Normal Operation

| Pattern                  | Meaning                                  |
|--------------------------|------------------------------------------|
| Solid green              | All sensors within threshold ±5°C        |
| Slow red blink (500 ms)  | One or more sensors outside threshold    |
| Fast red blink (250 ms)  | Sensor error or read failure             |

---

## Security Features

### Salted SHA256 Authentication

Passwords are never stored in plaintext. The system stores only `SHA256(SALT + PASSWORD)` in Flash.

All web endpoints require authentication: the root dashboard (`/`), temperature CSV (`/temp.csv`), humidity CSV (`/humid.csv`), threshold endpoint (`/threshold`), status endpoint (`/status`), sysinfo endpoint (`/sysinfo`), events log (`/events`), and archive downloads (`/archive`).

### IP-Based Connection Limiting

- **Global limit:** 8 simultaneous connections across all clients
- **Per-IP limit:** 3 simultaneous connections from any single IP
- **Idle timeout:** 5 minutes
- **Cleanup interval:** Every 30 seconds

### Hardware Watchdog

The 8-second hardware watchdog provides automatic crash recovery. If any part of the firmware stalls — network hang, SD deadlock, or unexpected loop — the Arduino reboots automatically without human intervention. All reboots are logged to `EVENTS.txt` with a timestamp and are visible in the Watchdog Alerts panel on the dashboard.

---

## Data Logging

- **Interval:** Every 5 minutes, strictly time-aligned to clock boundaries (xx:00, xx:05, etc.)
- **Files:** Daily files — `YYMMDD_T.csv` (temperature) and `YYMMDD_H.csv` (humidity)
- **Retention:** 180 days — automated file deletion occurs at midnight
- **Watchdog log:** `EVENTS.txt` — one timestamped entry per reboot
- **Format:** Date, Time, Sensor A, Sensor B, Sensor C, Sensor D
- **Time sync:** NTP updates every 24 hours — primary `192.168.80.8`, fallback `pool.ntp.org`
- **Timezone:** Eastern Time with automatic DST adjustment
- **DST Start:** 2nd Sunday of March at 2:00 AM (EDT, UTC-4)
- **DST End:** 1st Sunday of November at 2:00 AM (EST, UTC-5)
- **Persistence:** All data survives power outages

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
- Verify the data wire from each M12 connector is landed on the correct Arduino pin (A:40, B:41, C:30, D:31).
- Confirm GND is connected for all sensors.
- For M12 wiring: Pin 2 = Sensor A or C data, Pin 4 = Sensor B or D data, Pin 1 = VCC, Pin 3 = GND.
- The web dashboard sensor status bar will show a blinking red ERR dot for any failed sensors.

### Temperature threshold shows NaN or 0 on boot

- Occurs when EEPROM has never been written. System automatically resets to 42°C and writes to EEPROM on first boot.
- If it persists, use the button adjustment sequence to manually set and save a threshold value.

### System keeps rebooting

- Check the Watchdog Alerts panel on the dashboard for timestamped reboot entries.
- Common causes: SD card too slow or full, network instability, power supply brownout.
- Confirm SD card is formatted FAT32 and has adequate free space.

### Chart not rendering / blank chart area

- Open browser developer tools (F12) and check the console for JavaScript errors.
- Confirm the CDN scripts are loading — the device needs network access to `cdn.jsdelivr.net` for Chart.js, hammerjs, and the zoom plugin.
- Try clicking "Update Now" to force a data refresh.

### DHCP failed / cannot reach web interface

- Device falls back to static IP `192.168.48.20` — try that address first.
- Confirm Ethernet cable is connected and network has a DHCP server.
- Confirm firewall is not blocking port 80.

### Authentication fails after password setup

- Confirm `AUTH_SALT` in `config.h` exactly matches the salt used to generate the hash.
- Verify sketch was uploaded after saving `config.h`.

### Time not syncing / wrong time displayed

- System tries `192.168.80.8` first, then falls back to `pool.ntp.org`.
- Check Serial Monitor — shows which server responded or reports timeout on both.
- Verify port 123 (UDP) is not blocked by a firewall.

### Time is off by one hour

- Confirm you are running v1.10 or later which includes the DST month index fix.
- Serial Monitor will show `DST Active: Yes (EDT)` or `DST Active: No (EST)`.

### SD card initialization failed

- Confirm SD card is formatted as FAT32.
- Cards larger than 32GB may need to be reformatted — Windows defaults these to exFAT.
- Verify card is properly seated and CS pin wiring is correct (pin 4).

### Cannot safely remove SD card

- Use the "Prepare SD for Removal / Halt" button in the System Status panel on the dashboard.
- Wait for the LCD to show `SD UNMOUNTED / SAFE TO UNPLUG` before removing the card.
- Do not remove the card while the red LED is blinking — that indicates active SD write.

### CSV data lost after power outage

- Check SD card is properly inserted and formatted FAT32.
- The last data point written before the outage may be incomplete — all prior records are intact.
- Daily files are written independently, so only the current day's last entry is at risk.

---

## Version History

See [CHANGELOG.md](CHANGELOG.md) for full version history.

---

## Acknowledgments

Built for Seegrid aging room environmental monitoring. Uses Chart.js for web visualization, chartjs-plugin-zoom and hammerjs for chart interaction, and chartjs-adapter-date-fns for time axis formatting. NTP implementation based on Arduino examples. Security features implement OWASP best practices for password storage.
