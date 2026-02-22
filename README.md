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

- **Reads all four DHT22 sensors** every 2 seconds and updates the LCD display. The display rotates through sensor zones automatically.
- **Controls the LED indicators** based on sensor status — solid green when all sensors are within the threshold margin, slow red blink when one or more sensors are out of range, and fast red blink on a sensor read failure.
- **Logs sensor data to SD card** every 5 minutes, writing timestamped rows to `temp.csv` and `humid.csv`. Data survives power outages and can be downloaded directly from the web interface.
- **Serves a web dashboard** with interactive Chart.js charts showing temperature and humidity trends over 1, 3, 5, or 7 days. All endpoints are protected by HTTP Basic Auth using salted SHA256 password hashing.
- **Syncs time via NTP** from `time.nist.gov` at startup and every 24 hours thereafter, with automatic DST adjustment for Eastern Time.
- **Tracks network connections** per IP address to prevent resource exhaustion. A maximum of 8 simultaneous global connections and 3 per IP are enforced; connections idle for 5 minutes are released automatically.
- **Persists the temperature threshold** to EEPROM so the user-configured value survives power outages. Authentication credentials are stored in Flash memory and are never modified at runtime.

---

## Requirements

### Hardware

- Arduino Mega 2560 (or compatible)
- Ethernet Shield W5100 or W5500
- 4× DHT22 temperature/humidity sensors
- 20×4 I2C LCD display (I2C address `0x3F`)
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
| DHT22 Sensor A  | 2   | Digital      |
| DHT22 Sensor B  | 3   | Digital      |
| DHT22 Sensor C  | 5   | Digital      |
| DHT22 Sensor D  | 6   | Digital      |
| Green LED       | 7   | Digital      |
| Red LED         | 8   | Digital      |
| Ethernet CS     | 10  | SPI          |
| Button          | 13  | INPUT_PULLUP |
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
| `network.cpp`     | Connection tracking, NTP, and time functions |
| `sensors.h`       | Sensor and LED declarations |
| `sensors.cpp`     | DHT22 reading, LED control, and button handling |
| `display.h`       | LCD display declarations |
| `display.cpp`     | LCD initialization and display updates |
| `storage.h`       | SD card and web serving declarations |
| `storage.cpp`     | CSV logging and HTML generation |
| `README.md`       | This file |

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

Edit `Aging_Room.ino` lines 40–43 if you need to change the static IP fallback used when DHCP is unavailable:

```cpp
IPAddress ip(192, 168, 16, 70);      // Static IP
IPAddress gateway(192, 168, 16, 1);   // Gateway
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(192, 168, 16, 1);       // DNS server
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
  #define AUTH_SALT            "MyCompanySalt2026"
  #define AUTH_PASSWORD_SHA256 "7a8f9b2c3d4e5f6a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4"
```

---

### Password Security Guidelines

**DO:**
- ✓ Change the default salt `SeegridAgingRoom2026` to something unique per installation
- ✓ Use a strong password (12+ characters, mixed case, numbers, symbols)
- ✓ Use a different salt for each deployed unit
- ✓ Store your salt and password in a secure password manager

**DON'T:**
- ✗ Use the default salt in production
- ✗ Commit your actual password or salt to Git
- ✗ Add spaces or separators when combining salt and password
- ✗ Reuse salts across multiple deployments

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

### Flash Memory — Where Credentials Are Stored

Flash stores the compiled program code and all `#define` constants, including your authentication salt and password hash.

| Property | Value |
|----------|-------|
| Size | 256 KB |
| Current usage | 63,844 bytes (25%) |
| Survives power loss | **Yes** |
| Write method | Arduino IDE upload only |
| Runtime modification | Not possible |

Because credentials are in Flash, they cannot be changed by a running bug, network attack, or power event. Changing them requires uploading a new sketch via USB.

---

### EEPROM — Where the Temperature Threshold Is Stored

EEPROM stores only the user-configured temperature threshold (4 bytes). It is not used for credentials because it is too easily readable and writable at runtime.

| Property | Value |
|----------|-------|
| Size | 4 KB |
| Current usage | 4 bytes (0.1%) |
| Survives power loss | **Yes** |
| Write method | Button interface at runtime |

```cpp
// sensors.cpp — Reading threshold on startup
void initSensors() {
  EEPROM.get(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  if (tempThreshold < MIN_THRESHOLD || tempThreshold > MAX_THRESHOLD) {
    tempThreshold = DEFAULT_TEMP_THRESHOLD;
  }
}

// sensors.cpp — Saving threshold when user adjusts via button
void handleButtonPress() {
  EEPROM.put(0, tempThreshold);
}
```

---

### RAM — Volatile Working Memory

RAM holds all runtime variables (sensor readings, connection state, HTTP buffers) and is lost immediately on power loss. The system re-initializes everything from Flash and EEPROM at each boot.

| Property | Value |
|----------|-------|
| Size | 8 KB |
| Current usage | 3,893 bytes (47%) |
| Survives power loss | **No** |

---

### SD Card — Historical Sensor Data

The SD card stores `temp.csv` and `humid.csv`. Data accumulates continuously and survives power outages. The last write before a power loss may be incomplete but the rest of the file is unaffected.

| Property | Value |
|----------|-------|
| Size | User-supplied (typically 2–32 GB) |
| Survives power loss | **Yes** |
| Estimated growth | ~14 KB/day · ~5 MB/year |

---

### Memory Persistence Summary

| Memory Type | Size  | Your Usage    | Survives Power Loss | Credentials Stored |
|-------------|-------|---------------|---------------------|--------------------|
| **Flash**   | 256 KB | 63 KB (25%)  | **Yes**             | **Yes ← auth here** |
| **EEPROM**  | 4 KB  | 4 bytes (0.1%) | **Yes**            | No                 |
| **RAM**     | 8 KB  | 3.9 KB (47%) | **No**              | No                 |
| **SD Card** | User  | Growing       | **Yes**             | No                 |

---

### Power Outage Recovery Sequence

```
[0s]    Power on
[1s]    Flash loads program code and credentials
[2s]    EEPROM reads temperature threshold
[3s]    Display shows boot sequence
[5s]    Sensors initialize
[10s]   Network attempts DHCP
[15s]   Device IP address displayed on LCD
[20s]   NTP time sync begins
[25s]   System fully operational
[30s]   First sensor reading logged to SD card
```

No user action is required after a power outage. Credentials, threshold settings, and all historical data are automatically available.

---

### Memory Health Indicators

| Memory | Healthy | Warning | Critical |
|--------|---------|---------|----------|
| Flash  | < 80%   | 80–95%  | > 95%    |
| EEPROM | < 50%   | 50–90%  | > 90%    |
| RAM    | < 70%   | 70–85%  | > 85%    |
| SD Card free | > 100 MB | 10–100 MB | < 10 MB |

Your current system is well within healthy ranges on all metrics.

---

## Usage

### Finding the Device IP Address

After boot, the device displays its IP address on the LCD for 10 seconds and on the Serial Monitor (9600 baud). The device attempts DHCP first and falls back to the static IP `192.168.16.70` if DHCP fails.

### Accessing the Web Interface

1. Navigate to `http://[DEVICE_IP]` in your browser.
2. Enter credentials when prompted:
   - **Username:** `Seegrid`
   - **Password:** Your configured password

### Web Dashboard Features

- **Temperature tab** — View all sensor readings with threshold lines
- **Humidity tab** — View humidity trends across all four sensors
- **Time range selector** — Choose 1, 3, 5, or 7 day views
- **Export PNG** — Download a chart image
- **Download CSV** — Get raw `temp.csv` or `humid.csv` files
- **Delete CSV** — Clear historical data (requires confirmation)
- **Auto-refresh** — Charts update every 5 minutes automatically

---

## Adjusting the Temperature Threshold

The threshold is adjusted using the physical button on the device. The full process takes approximately 35 seconds.

1. Press and hold the button for **5 seconds**.
2. Both LEDs alternate at 250 ms to confirm you are in adjustment mode.
3. Continue holding — the threshold increases by 1°C every 2 seconds, cycling between 20°C and 50°C. The current value is shown on the LCD.
4. Release the button when the desired threshold is displayed.
5. The green LED flashes 10 times to confirm the save.
6. Both LEDs flash 20 times as a final confirmation.
7. The display shows the old and new threshold values for 10 seconds.

The new threshold is saved to EEPROM and persists through power outages.

### LED Behavior During Adjustment

| Phase | Green LED | Red LED | Duration |
|-------|-----------|---------|----------|
| Holding button (0–5 s) | Alternates 250 ms | Alternates 250 ms | 5 seconds |
| Entry confirmation | Flashes 10× | OFF | ~5 seconds |
| Adjusting (holding) | Blinks 250 ms | OFF | Until release |
| Save confirmation | OFF | Flashes 10× | ~5 seconds |
| Final confirmation | Flashes 20× | Flashes 20× | ~20 seconds |
| Return to normal | Status dependent | Status dependent | Ongoing |

### LED Status During Normal Operation

| Pattern | Meaning |
|---------|---------|
| Solid green | All sensors within threshold ±3°C |
| Slow red blink (500 ms) | One or more sensors outside threshold |
| Fast red blink (250 ms) | Sensor error or read failure |

---

## Security Features

### Salted SHA256 Authentication

Passwords are never stored in plaintext. The system stores only `SHA256(SALT + PASSWORD)` in Flash. Because the salt is unique per installation, pre-computed hash databases (rainbow tables) are useless even if someone extracts the Flash contents.

All web endpoints require authentication: the root dashboard (`/`), temperature CSV download (`/temp.csv`), humidity CSV download (`/humid.csv`), and file deletion (`/delete_temp`, `/delete_humid`).

---

### IP-Based Connection Limiting

The server tracks connections by IP address in a 15-slot array and enforces the following limits:

- **Global limit:** 8 simultaneous connections across all clients
- **Per-IP limit:** 3 simultaneous connections from any single IP
- **Idle timeout:** Connections inactive for 5 minutes are released automatically
- **Cleanup interval:** Stale connections are purged every 30 seconds

Connections that exceed either limit receive an HTTP 503 response with a `Retry-After: 60` header. Connection activity is reported on the Serial Monitor:

```
Connection accepted. IP: 192.168.1.100 | Global: 3/8
Per-IP limit reached for: 192.168.1.100 (3 connections)
Connection released. IP: 192.168.1.100 | Global: 2/8
```

---

### Additional Security Measures

- **Request size limiting** — 512-byte maximum prevents buffer overflow attacks
- **Request timeout** — 5-second per-request timeout prevents slowloris attacks
- **HTTP Basic Auth** — Industry-standard authentication protocol
- **No default credentials** — System requires password setup before first use
- **Local network only** — Designed for internal facility use, not internet exposure

---

### Security Best Practices for Deployment

1. **Change the default salt** — Never use `SeegridAgingRoom2026` in production.
2. **Use strong passwords** — Minimum 12 characters, mixed case, numbers, and symbols.
3. **Unique salt per installation** — Each deployed unit should have a different salt.
4. **Network isolation** — Deploy on an isolated VLAN or private network segment.
5. **Physical security** — Secure the Arduino in a locked enclosure.
6. **Backup credentials** — Store your salt and password in a secure password manager.
7. **Review access logs** — Monitor Serial Monitor output for unauthorized login attempts.

---

## Data Logging

- **Interval:** Every 5 minutes
- **Files:** `temp.csv` and `humid.csv` on SD card
- **Format:** Date, Time, Sensor A, Sensor B, Sensor C, Sensor D
- **Time sync:** NTP updates every 24 hours from `time.nist.gov`
- **Timezone:** Eastern Time with automatic DST adjustment
- **Persistence:** All data survives power outages

---

## Configuration Constants

Edit `config.h` to change these parameters:

```cpp
#define MIN_THRESHOLD          20       // Minimum threshold (°C)
#define MAX_THRESHOLD          50       // Maximum threshold (°C)
#define DEFAULT_TEMP_THRESHOLD 20       // Default threshold (°C)
#define THRESHOLD_MARGIN       3.0      // Alert margin (±°C)
#define MAX_GLOBAL_CONNECTIONS 8        // Total connection limit
#define MAX_PER_IP_CONNECTIONS 3        // Per-IP connection limit
#define CONNECTION_TIMEOUT     300000   // 5 minutes (ms)
#define SENSOR_READ_INTERVAL   2000     // 2 seconds (ms)
#define CSV_WRITE_INTERVAL     300000   // 5 minutes (ms)
#define NTP_INTERVAL           86400000 // 24 hours (ms)
```

---

## Troubleshooting

### SD card initialization failed

- Confirm the SD card is formatted as FAT32.
- Check the SD card module wiring (CS pin 4).
- Verify the card is properly seated.
- Try a different SD card.

### DHCP failed / cannot reach web interface

- The device falls back to static IP `192.168.16.70` when DHCP fails — try that address first.
- Confirm the Ethernet cable is connected and the network has a DHCP server.
- Verify you are on the same network segment as the device.
- Confirm your firewall is not blocking port 80.

### Sensor reading errors / LCD shows "ERR"

- Check DHT22 sensor wiring and confirm 5 V power is reaching the sensors.
- Ensure pull-up resistors are present on the data lines (usually built into modules).
- Verify the correct pins are used (A: 2, B: 3, C: 5, D: 6).

### Authentication fails after password setup

- Confirm `AUTH_SALT` in `config.h` exactly matches the salt you used to generate the hash.
- Verify you uploaded the sketch after saving your changes to `config.h`.
- Regenerate the hash using `generate_salted_hash.ino` if you are uncertain.
- Check the Serial Monitor for authentication error messages.

### Time not syncing / wrong time displayed

- Verify the device has internet access and the NTP server `129.6.15.28` is reachable.
- Check the Serial Monitor for NTP response messages.
- NTP sync occurs at startup and every 24 hours — wait up to 30 seconds after boot.

### Temperature threshold resets to 20°C after reboot

This should not happen — the threshold is stored in EEPROM. If it does reset:
- EEPROM may be corrupted (rare). Re-set the threshold via the button to write a fresh value.
- Confirm the stored value is within the valid range (20–50°C).

### Credentials not working after power outage

Credentials are stored in Flash memory, which survives power loss indefinitely. If login fails after an outage:
- Confirm you originally uploaded the sketch with the correct credentials.
- Flash corruption is extremely rare. If suspected, re-upload the sketch with your `config.h` values.

### CSV data lost after power outage

- Check that the SD card is properly inserted.
- Verify the card is formatted as FAT32.
- The last data point written before the outage may be incomplete — all prior records are intact.

---

## Version History

- **v1.1 — Security Update (Current)**
  - Implemented salted SHA256 password hashing
  - Added rainbow table attack protection
  - Improved password security documentation
  - Created `generate_salted_hash.ino` password hash generation tool
  - Added comprehensive memory architecture documentation

- **v1.0 — Initial Release**
  - Four-sensor temperature and humidity monitoring
  - Web dashboard with interactive Chart.js charts
  - CSV data logging to SD card
  - NTP time synchronization with DST support
  - Basic SHA256 authentication
  - IP-based connection rate limiting

---

## Acknowledgments

Built for Seegrid aging room environmental monitoring. Uses Chart.js for web visualization. NTP implementation based on Arduino examples. Security features implement OWASP best practices for password storage.
