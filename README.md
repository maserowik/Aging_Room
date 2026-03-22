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
- **Serves a web dashboard** with interactive Chart.js charts showing temperature and humidity trends over 1, 3, 5, or 7 days. The chart resolution scales automatically with the selected time range — every 5 minutes for 1 day, every 30 minutes for 3 days, and every 60 minutes for 5 or 7 days. All endpoints are protected by HTTP Basic Auth using salted SHA256 password hashing. The dashboard polls the device every 30 seconds for threshold changes and updates the chart lines instantly without a full page reload.
- **Syncs time via NTP** at startup and every 24 hours thereafter. The system first contacts the internal NTP server `192.168.80.8` and falls back to the public NTP pool `pool.ntp.org` if the internal server is unreachable. DST transitions occur correctly at 2:00 AM on the 2nd Sunday of March (EDT) and 1st Sunday of November (EST).
- **Tracks network connections** per IP address to prevent resource exhaustion. A maximum of 8 simultaneous global connections and 3 per IP are enforced; connections idle for 5 minutes are released automatically.
- **Persists the temperature threshold** to EEPROM so the user-configured value survives power outages. Authentication credentials are stored in Flash memory and are never modified at runtime.

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
  if (isnan(tempThreshold) || tempThreshold < MIN_THRESHOLD || tempThreshold > MAX_THRESHOLD) {
    tempThreshold = DEFAULT_TEMP_THRESHOLD;
    EEPROM.put(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  }
}

// sensors.cpp — Saving threshold when user adjusts via button
void handleButtonPress() {
  EEPROM.put(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
}
```

---

### RAM — Volatile Working Memory

RAM holds all runtime variables (sensor readings, connection state, HTTP buffers) and is lost immediately on power loss. The system re-initializes everything from Flash and EEPROM at each boot.

| Property | Value |
|----------|-------|
| Size | 8 KB |
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

| Memory Type | Size   | Survives Power Loss | Credentials Stored  |
|-------------|--------|---------------------|---------------------|
| **Flash**   | 256 KB | **Yes**             | **Yes ← auth here** |
| **EEPROM**  | 4 KB   | **Yes**             | No                  |
| **RAM**     | 8 KB   | **No**              | No                  |
| **SD Card** | User   | **Yes**             | No                  |

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
[20s]   NTP time sync begins (primary then fallback)
[25s]   System fully operational
[30s]   First sensor reading logged to SD card
```

No user action is required after a power outage. Credentials, threshold settings, and all historical data are automatically available.

---

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

After boot, the device displays its IP address on the LCD for 10 seconds and on the Serial Monitor (115200 baud). The device attempts DHCP first and falls back to the static IP `192.168.48.20` if DHCP fails.

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
- **Live threshold lines** — Chart threshold lines update within 30 seconds of any button adjustment, no page reload required

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
4. **Keep holding** — the threshold increases by 1°C every 2 seconds, cycling between 37°C and 47°C. The current value is shown on the LCD.
5. **Release the button** when the desired threshold is displayed to save.
6. Red LED flashes 10 times to confirm the save.
7. Both LEDs flash 20 times as a final confirmation.
8. The display shows the old and new threshold values for 10 seconds.
9. The web dashboard threshold lines update automatically within 30 seconds.

The new threshold is saved to EEPROM and persists through power outages.

### LED Behavior During Adjustment

| Phase                   | Green LED          | Red LED            | Duration      |
|-------------------------|--------------------|--------------------|---------------|
| Holding button (0–5 s)  | Alternates 250 ms  | Alternates 250 ms  | 5 seconds     |
| Entry confirmation      | Flashes 10×        | OFF                | ~5 seconds    |
| Adjusting (holding)     | Blinks 250 ms      | OFF                | Until release |
| Save confirmation       | OFF                | Flashes 10×        | ~5 seconds    |
| Final confirmation      | Flashes 20×        | Flashes 20×        | ~10 seconds   |
| Return to normal        | Status dependent   | Status dependent   | Ongoing       |

### LED Status During Normal Operation

| Pattern                  | Meaning                                  |
|--------------------------|------------------------------------------|
| Solid green              | All sensors within threshold ±5°C        |
| Slow red blink (500 ms)  | One or more sensors outside threshold    |
| Fast red blink (250 ms)  | Sensor error or read failure             |

---

## Security Features

### Salted SHA256 Authentication

Passwords are never stored in plaintext. The system stores only `SHA256(SALT + PASSWORD)` in Flash. Because the salt is unique per installation, pre-computed hash databases (rainbow tables) are useless even if someone extracts the Flash contents.

All web endpoints require authentication: the root dashboard (`/`), temperature CSV download (`/temp.csv`), humidity CSV download (`/humid.csv`), file deletion (`/delete_temp`, `/delete_humid`), and the threshold endpoint (`/threshold`).

---

### IP-Based Connection Limiting

The server tracks connections by IP address in a 15-slot array and enforces the following limits:

- **Global limit:** 8 simultaneous connections across all clients
- **Per-IP limit:** 3 simultaneous connections from any single IP
- **Idle timeout:** Connections inactive for 5 minutes are released automatically
- **Cleanup interval:** Stale connections are purged every 30 seconds

Connections that exceed either limit receive an HTTP 503 response with a `Retry-After: 60` header.

---

### Additional Security Measures

- **Request size limiting** — 1024-byte maximum prevents buffer overflow attacks
- **Request timeout** — 5-second per-request timeout prevents slowloris attacks
- **HTTP Basic Auth** — Industry-standard authentication protocol
- **No default credentials** — System requires password setup before first use
- **Local network only** — Designed for internal facility use, not internet exposure

---

### Security Best Practices for Deployment

1. **Change the default salt** — Never use the example salt in production.
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
- **Time sync:** NTP updates every 24 hours — primary server `192.168.80.8`, fallback `pool.ntp.org`
- **Timezone:** Eastern Time with automatic DST adjustment
- **DST Start:** 2nd Sunday of March at 2:00 AM (EDT, UTC-4)
- **DST End:** 1st Sunday of November at 2:00 AM (EST, UTC-5)
- **Persistence:** All data survives power outages

---

## Configuration Constants

Edit `config.h` to change these parameters:

```cpp
#define MIN_THRESHOLD          37       // Minimum adjustable threshold (°C)
#define MAX_THRESHOLD          47       // Maximum adjustable threshold (°C)
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
- If using bare DHT22 sensors (not modules), ensure a 4.7kΩ–10kΩ pull-up resistor is present on each data line.

### Temperature threshold shows NaN or 0 on boot

- This occurs when EEPROM has never been written. The system automatically resets to 42°C and writes a valid value to EEPROM. This will only happen once on a fresh board.
- If it persists, use the button adjustment sequence to manually set and save a threshold value.

### Temperature threshold resets after reboot

- The threshold is stored in EEPROM and survives power loss. If it resets, EEPROM may be corrupted (rare).
- Re-set the threshold via the button to write a fresh value.
- Confirm the stored value is within the valid range (37–47°C).

### DHCP failed / cannot reach web interface

- The device falls back to static IP `192.168.48.20` when DHCP fails — try that address first.
- Confirm the Ethernet cable is connected and the network has a DHCP server.
- Verify you are on the same network segment as the device.
- Confirm your firewall is not blocking port 80.

### Authentication fails after password setup

- Confirm `AUTH_SALT` in `config.h` exactly matches the salt you used to generate the hash.
- Verify you uploaded the sketch after saving your changes to `config.h`.
- Check the Serial Monitor for authentication error messages.

### Time not syncing / wrong time displayed

- The system tries the internal NTP server `192.168.80.8` first, then falls back to `pool.ntp.org`.
- Check the Serial Monitor — it will show which server responded or report timeout on both.
- If both servers time out, the device will continue running but timestamps will be incorrect until the next NTP retry 24 hours later.
- Verify the device has network access and that port 123 (UDP) is not blocked by a firewall.

### Time is off by one hour

- This indicates a DST detection failure. Confirm you are running v1.5 or later which includes the corrected DST calculation.
- Check the Serial Monitor output — it will show `DST Active: Yes (EDT)` or `DST Active: No (EST)`.

### SD card initialization failed

- Confirm the SD card is formatted as FAT32.
- Check the SD card module wiring (CS pin 4).
- Verify the card is properly seated.
- Try a different SD card. Cards larger than 32GB may need to be reformatted as FAT32 — Windows defaults these to exFAT which is not compatible.

### CSV data lost after power outage

- Check that the SD card is properly inserted.
- Verify the card is formatted as FAT32.
- The last data point written before the outage may be incomplete — all prior records are intact.

### Credentials not working after power outage

- Credentials are stored in Flash memory, which survives power loss indefinitely.
- Confirm you originally uploaded the sketch with the correct credentials.
- If suspected corruption, re-upload the sketch with your `config.h` values.

---

## Version History

See [CHANGELOG.md](CHANGELOG.md) for full version history.

---

## Acknowledgments

Built for Seegrid aging room environmental monitoring. Uses Chart.js for web visualization. NTP implementation based on Arduino examples. Security features implement OWASP best practices for password storage.
