# Seegrid Aging Room Environmental Monitoring System

Arduino-based temperature and humidity monitoring system with web interface, data logging, and real-time alerts for industrial aging room environments.

## Features

- **Four DHT22 Sensors**: Simultaneous monitoring of temperature and humidity across multiple zones
- **Real-time LCD Display**: 20x4 I2C LCD showing live sensor readings with automatic screen rotation
- **Web Dashboard**: Interactive charts with configurable time ranges (1, 3, 5, 7 days)
- **CSV Data Logging**: Automatic timestamped data capture every 5 minutes to SD card
- **Visual Alerts**: Red/Green LED indicators with distinct blink patterns for error states
- **NTP Time Sync**: Automatic network time synchronization with DST support
- **Security**: Salted SHA256 password authentication and connection rate limiting
- **Adjustable Threshold**: Physical button interface for on-device temperature threshold configuration
- **Non-Volatile Storage**: Critical settings persist through power outages

## Hardware Requirements

- Arduino Mega 2560 (or compatible)
- Ethernet Shield W5100/W5500
- 4x DHT22 Temperature/Humidity Sensors
- 20x4 I2C LCD Display (0x3F address)
- SD Card Module
- Red and Green LEDs
- Push Button
- MicroSD Card (formatted FAT32)

## Pin Configuration

| Component | Pin | Notes |
|-----------|-----|-------|
| DHT22 Sensor A | 2 | Digital |
| DHT22 Sensor B | 3 | Digital |
| DHT22 Sensor C | 5 | Digital |
| DHT22 Sensor D | 6 | Digital |
| Green LED | 7 | Digital |
| Red LED | 8 | Digital |
| Ethernet CS | 10 | SPI |
| Button | 13 | INPUT_PULLUP |
| SD Card CS | 4 | SPI |
| LCD | I2C | SDA/SCL |

## Installation

### 1. Install Required Libraries

Open Arduino IDE and install these libraries via Library Manager:

- DHT sensor library by Adafruit
- Adafruit Unified Sensor
- LiquidCrystal I2C
- Ethernet (built-in)
- SD (built-in)
- Crypto by Rhys Weatherley
- arduino-base64 by Densaugeo

### 2. Clone Repository

```bash
git clone https://gitlab.com/seegrid/quality/Aging_Room
cd Aging_Room
```

### 3. Configure Network

Edit `Aging_Room.ino` lines 40-43 if you need to change the static IP fallback:

```cpp
IPAddress ip(192, 168, 16, 70);      // Static IP
IPAddress gateway(192, 168, 16, 1);   // Gateway
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(192, 168, 16, 1);       // DNS server
```

### 4. Set Authentication Password (CRITICAL SECURITY STEP)

**Default Credentials:**
- Username: `Seegrid`
- Password: **You MUST set this before deployment**

#### Understanding Salted Password Hashing

This system uses **salted SHA256 hashing** for authentication, which provides strong protection against rainbow table attacks.

**What is a Salt?**
A salt is a unique, random string added to your password before hashing. This ensures that even if two users have the same password, their hashes will be completely different.

**Example:**
- Without salt: `SHA256("MyPassword")` = same hash every time
- With salt: `SHA256("UniqueSalt123" + "MyPassword")` = unique hash per installation

**Why This Matters:**
- **Rainbow Table Protection**: Pre-computed hash tables are useless without knowing your unique salt
- **Same Password, Different Hashes**: Your "admin123" password has a different hash than someone else's "admin123"
- **Industry Best Practice**: Recommended by OWASP and security standards

---

#### Setting Your Salted Password

**Step-by-Step Instructions:**

1. **Choose Your Security Values:**
   - Unique Salt: `SeegridPittsburgh2026` (example - make yours unique)
   - Strong Password: `MySecure!Pass2026` (example - use your own)

2. **Manually Concatenate** (combine) salt and password:
   ```
   SeegridPittsburgh2026MySecure!Pass2026
   ```
   **Important:** Salt first, then password, no spaces or separators

3. **Generate SHA256 Hash:**
   - Visit: https://emn178.github.io/online-tools/sha256.html
   - Paste the **combined string** from step 2
   - Copy the resulting 64-character hash

4. **Update `config.h`:**
   ```cpp
   #define AUTH_SALT "SeegridPittsburgh2026"              // Your salt from step 1
   #define AUTH_PASSWORD_SHA256 "your_copied_hash_here"   // Hash from step 3
   ```

5. **Save and Upload** the sketch to your Arduino

**Example Walkthrough:**

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
  #define AUTH_SALT "MyCompanySalt2026"
  #define AUTH_PASSWORD_SHA256 "7a8f9b2c3d4e5f6a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4"
```

---

#### Security Best Practices

**DO:**
- ✓ Change the default salt `SeegridAgingRoom2026` to something unique
- ✓ Use a strong password (12+ characters, mix of letters, numbers, symbols)
- ✓ Keep your salt and password private
- ✓ Use different salts for different installations
- ✓ Document your salt/password in a secure password manager

**DON'T:**
- ✗ Use the default salt in production
- ✗ Commit your actual password or salt to Git
- ✗ Share your salt publicly
- ✗ Use common passwords like "password123"
- ✗ Reuse salts across multiple deployments
- ✗ Add spaces or separators when combining salt and password

**Example Strong Configurations:**

```cpp
// Example 1: Company-based
Salt: "Seegrid_PA_Room4_2026"
Password: "Ag!ngR00m$ecur3"
Combined: Seegrid_PA_Room4_2026Ag!ngR00m$ecur3
(Generate SHA256 of combined string)

// Example 2: Location-based  
Salt: "PittsburghFacility_Jan2026"
Password: "M0n!t0r$ystem#2026"
Combined: PittsburghFacility_Jan2026M0n!t0r$ystem#2026
(Generate SHA256 of combined string)

// Example 3: Random
Salt: "x7K9mP3qL2wR8nT5"
Password: "K#8pL@5mQ!2wE9rT"
Combined: x7K9mP3qL2wR8nT5K#8pL@5mQ!2wE9rT
(Generate SHA256 of combined string)
```

---

#### Important Notes

**The Current Placeholder Will NOT Work:**
The hash in `config.h` is just a placeholder:
```cpp
#define AUTH_PASSWORD_SHA256 "8b3d7f4a1c2e9f6b..."  // This is INVALID
```
You MUST generate a new salted hash before deployment.

**Regenerating Your Password:**
If you need to change your password:
1. Keep the same salt OR choose a new one
2. Combine new salt+password
3. Generate SHA256 hash at website
4. Update both values in `config.h`
5. Re-upload to Arduino

**Verification:**
To verify your hash is correct, you can regenerate it:
- Use the same salt and password
- Combine them the same way
- Generate hash again
- Compare - they should match exactly

### 5. Upload to Arduino

1. Open `Aging_Room.ino` in Arduino IDE
2. Select your board (Arduino Mega 2560)
3. Select the correct COM port
4. Click Upload

## File Structure

```
Aging_Room/
├── Aging_Room.ino       # Main program with setup() and loop()
├── config.h             # All configuration constants and includes
├── auth.h               # Authentication function declarations
├── auth.cpp             # Salted SHA256 password validation logic
├── network.h            # Network and NTP declarations
├── network.cpp          # Connection tracking, NTP, time functions
├── sensors.h            # Sensor and LED declarations
├── sensors.cpp          # DHT22 reading, LED control, button handling
├── display.h            # LCD display declarations
├── display.cpp          # LCD initialization and display updates
├── storage.h            # SD card and web serving declarations
├── storage.cpp          # CSV logging and HTML generation
├── .gitignore           # Git ignore patterns
└── README.md            # This file
```

## Memory Architecture and Data Persistence

### Flash Memory (Program Storage)

**What's Stored:**
- All program code (setup, loop, functions)
- Authentication credentials (username, salt, password hash)
- Configuration constants (#define values)
- HTML/CSS for web interface

**Characteristics:**
- **Size**: 256KB on Arduino Mega
- **Persistence**: 100,000+ years retention
- **Survives Power Loss**: YES ✓
- **Write Cycles**: ~10,000 times (only written during upload)
- **Modification**: Requires re-uploading sketch

**Your Usage:** 63,844 bytes (25%) - Plenty of room

**Security Benefit:** Authentication credentials in Flash are:
- More difficult to extract than EEPROM
- Protected from runtime modification
- Require physical access and special tools to read

---

### EEPROM (Electrically Erasable Programmable Read-Only Memory)

**What's Stored:**
- Temperature threshold setting (user-adjustable)

**Characteristics:**
- **Size**: 4KB on Arduino Mega
- **Persistence**: 10-20 years retention
- **Survives Power Loss**: YES ✓
- **Write Cycles**: ~100,000 times per address
- **Modification**: Can be changed at runtime (via button press)

**Your Usage:** 4 bytes (0.1%) for temperature threshold

**User Benefit:** Temperature setting persists through:
- Power outages
- System reboots
- Sketch re-uploads (EEPROM is preserved)

---

### RAM (Random Access Memory)

**What's Stored:**
- Runtime variables (sensor readings, network connections)
- String buffers, HTML generation
- Active connection tracking

**Characteristics:**
- **Size**: 8KB on Arduino Mega
- **Persistence**: Lost on power loss ✗
- **Survives Power Loss**: NO
- **Speed**: Fastest memory type

**Your Usage:** 3,893 bytes (47%) - Healthy headroom

---

### SD Card (External Storage)

**What's Stored:**
- CSV data files (temp.csv, humid.csv)
- Historical sensor readings
- Timestamped measurements

**Characteristics:**
- **Size**: Limited by SD card (typically 2-32GB)
- **Persistence**: Years (if card doesn't fail)
- **Survives Power Loss**: YES ✓
- **Removal**: Can be removed for data backup

**Your Usage:** Grows over time based on `max_days` setting

---

### Memory Summary Table

| Memory Type | Size | Your Usage | Survives Power Loss | What's Stored |
|-------------|------|------------|---------------------|---------------|
| **Flash** | 256KB | 63KB (25%) | YES ✓ | Code, auth credentials |
| **EEPROM** | 4KB | 4 bytes (0.1%) | YES ✓ | Temperature threshold |
| **RAM** | 8KB | 3.9KB (47%) | NO ✗ | Runtime data |
| **SD Card** | User-supplied | Growing | YES ✓ | Sensor logs (CSV files) |

---

### Why This Architecture is Secure

1. **Authentication in Flash**: Credentials can't be changed without re-uploading code
2. **Salted Hashing**: Even if Flash is dumped, password can't be easily recovered
3. **EEPROM for Settings Only**: User-adjustable data (not security-critical)
4. **Separation of Concerns**: Security data separate from runtime data

## Usage

### Finding Device IP Address

After boot, the device will display its IP address on:
1. LCD screen for 10 seconds
2. Serial Monitor (9600 baud)

The device attempts DHCP first, falls back to `192.168.16.70` if DHCP fails.

### Accessing Web Interface

1. Navigate to `http://[DEVICE_IP]` in your browser
2. Enter credentials when prompted:
   - Username: `Seegrid`
   - Password: [Your configured password]

### Web Dashboard Features

- **Temperature Tab**: View all sensor readings with threshold lines
- **Humidity Tab**: View humidity trends across all sensors
- **Time Range Selection**: Choose 1, 3, 5, or 7 day views
- **Export PNG**: Download chart images
- **Download CSV**: Get raw data files
- **Delete CSV**: Clear historical data (requires confirmation)
- **Auto-refresh**: Charts update every 5 minutes

### Adjusting Temperature Threshold

1. Press and hold the button for 5 seconds
2. Green LED will flash rapidly (10 times) to confirm adjustment mode
3. Keep holding the button
4. Threshold increases by 1°C every 2 seconds (cycles 20-50°C)
5. LCD shows current threshold value
6. Release button to save
7. Red LED flashes rapidly (10 times) to confirm save
8. Device displays old and new threshold for 10 seconds

**LED Behavior During Adjustment:**

| Phase | Green LED | Red LED | Duration |
|-------|-----------|---------|----------|
| Holding button (0-5s) | Alternates 250ms | Alternates 250ms | 5 seconds |
| Entry confirmation | Flashes 10 times | OFF | ~5 seconds |
| Adjusting (holding) | Blinks 250ms | OFF | Until release |
| Save confirmation | OFF | Flashes 10 times | ~5 seconds |
| Final confirmation | Flashes 20 times | Flashes 20 times | ~20 seconds |
| Return to normal | Status dependent | Status dependent | Ongoing |

**Total adjustment process: ~35 seconds minimum**

**Note:** The new threshold is saved to EEPROM and persists through power outages.

### LED Indicator Meanings

| Pattern | Meaning |
|---------|---------|
| Solid Green | All sensors within threshold ±3°C |
| Slow Red Blink (500ms) | One or more sensors outside threshold |
| Fast Red Blink (250ms) | Sensor error or read failure |

## Security Features

### Authentication (Salted SHA256 Hashing)

**Implementation:**
- Passwords are combined with a unique salt before hashing
- Salt is prepended to password: `SHA256(SALT + PASSWORD)`
- Only the resulting hash is stored in Flash memory
- Plaintext passwords never stored anywhere on the device

**Protection Against:**
- ✓ Rainbow table attacks (pre-computed hash databases)
- ✓ Dictionary attacks (common password lists)
- ✓ Brute force attacks (computationally expensive to crack)
- ✓ Password reuse across systems (unique hashes per installation)

**Security Level:**
- Meets OWASP password storage recommendations
- Industry-standard cryptographic hashing (SHA256)
- Additional salt layer prevents cross-installation attacks

**All Protected Endpoints:**
- Root page (`/`)
- Temperature CSV download (`/temp.csv`)
- Humidity CSV download (`/humid.csv`)
- File deletion (`/delete_temp`, `/delete_humid`)

---

### IP-Based Connection Limiting

The system implements sophisticated connection tracking to prevent denial-of-service attacks and resource exhaustion:

**How It Works:**
1. **Connection Tracking**: Each incoming connection is tracked by IP address in a 15-slot array
2. **Global Limit**: Maximum of 8 simultaneous connections across all clients
3. **Per-IP Limit**: Maximum of 3 simultaneous connections from any single IP address
4. **Timeout Management**: Connections that remain idle for 5 minutes are automatically released
5. **Automatic Cleanup**: Stale connections are purged every 30 seconds

**What Happens When Limits Are Reached:**
- New connections from rate-limited IPs receive HTTP 503 (Service Unavailable)
- "Retry-After: 60" header suggests clients wait before retrying
- Connection slots are released immediately when clients disconnect properly
- Failed or abandoned connections are cleaned up automatically

**Connection States:**
- **Accepted**: IP has available slots, global limit not reached
- **Rejected**: Either per-IP limit (3) or global limit (8) exceeded
- **Released**: Client disconnected, slot freed for reuse
- **Timeout**: Connection idle for 5+ minutes, automatically released

**Monitoring:**
Serial output shows real-time connection tracking:
```
Connection accepted. IP: 192.168.1.100 | Global: 3/8
Per-IP limit reached for: 192.168.1.100 (3 connections)
Connection released. IP: 192.168.1.100 | Global: 2/8
```

This prevents a single malicious client from monopolizing the server or overwhelming the Arduino's limited resources.

---

### Additional Security Measures

- **Request Size Limiting**: 512 byte maximum request size prevents buffer overflow attacks
- **Request Timeout**: 5 second timeout per request prevents slowloris attacks
- **HTTP Basic Auth**: Industry-standard authentication protocol
- **No Default Credentials**: System requires password setup before use
- **Local Network Only**: Designed for internal facility use, not internet exposure

---

### Security Best Practices for Deployment

1. **Change Default Salt**: Never use "SeegridAgingRoom2026" in production
2. **Use Strong Passwords**: Minimum 12 characters, mixed case, numbers, symbols
3. **Unique Per Installation**: Each deployed system should have different salt
4. **Network Isolation**: Deploy on isolated VLAN or private network segment
5. **Physical Security**: Secure Arduino in locked enclosure
6. **Regular Updates**: Monitor for security updates to libraries
7. **Access Logging**: Review Serial Monitor logs for unauthorized attempts
8. **Backup Credentials**: Store salt/password in secure password manager

## Data Logging

- **Interval**: Every 5 minutes (300,000ms)
- **Files**: `temp.csv` and `humid.csv` on SD card
- **Format**: Date, Time, Sensor A, Sensor B, Sensor C, Sensor D
- **Time Sync**: NTP updates every 24 hours from time.nist.gov
- **Timezone**: Eastern Time (EST/EDT with automatic DST)
- **Persistence**: Data survives power outages (stored on SD card)

## Configuration Constants

Edit `config.h` to modify these parameters:

```cpp
#define MIN_THRESHOLD 20                  // Minimum threshold (°C)
#define MAX_THRESHOLD 50                  // Maximum threshold (°C)
#define DEFAULT_TEMP_THRESHOLD 20         // Default threshold (°C)
#define THRESHOLD_MARGIN 3.0              // Alert margin (±°C)
#define MAX_GLOBAL_CONNECTIONS 8          // Total connection limit
#define MAX_PER_IP_CONNECTIONS 3          // Per-IP connection limit
#define CONNECTION_TIMEOUT 300000         // 5 minutes
#define SENSOR_READ_INTERVAL 2000         // 2 seconds
#define CSV_WRITE_INTERVAL 300000         // 5 minutes
#define NTP_INTERVAL 86400000             // 24 hours
```

## Troubleshooting

### SD Card Initialization Failed
- Ensure SD card is formatted as FAT32
- Check SD card module wiring (CS pin 4)
- Verify SD card is properly inserted
- Try a different SD card

### DHCP Failed
- Check Ethernet cable connection
- Verify network has DHCP server
- Device will fall back to static IP `192.168.16.70`

### Sensor Reading Errors
- Check DHT22 sensor connections
- Verify 5V power to sensors
- Ensure pull-up resistors on data lines (usually built into modules)
- LCD will display "ERR" for failed sensors

### Cannot Access Web Interface
- Verify device IP address on LCD or Serial Monitor
- Check that you're on the same network
- Ensure firewall isn't blocking port 80
- Verify correct username and password
- Check that you've set the salted password correctly

### Authentication Fails After Password Setup
- Verify `AUTH_SALT` in config.h matches the salt used to generate the hash
- Ensure you uploaded the code after changing config.h
- Regenerate hash using `generate_salted_hash.ino` if uncertain
- Check Serial Monitor for authentication errors

### Time Not Syncing
- Check internet connectivity
- Verify NTP server is accessible (129.6.15.28)
- Check Serial Monitor for NTP response messages

### Temperature Threshold Resets After Power Loss
- This should NOT happen - threshold is stored in EEPROM
- If it does reset, EEPROM may be corrupted
- Try manually setting threshold again via button

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/improvement`)
3. Commit your changes (`git commit -am 'Add new feature'`)
4. Push to the branch (`git push origin feature/improvement`)
5. Create a Pull Request

## License

This project is provided as-is for industrial monitoring applications.

## Support

For issues, questions, or contributions, please open an issue on GitHub.

## Version History

- **v1.1** - Security Update (Current)
  - Implemented salted SHA256 password hashing
  - Added rainbow table attack protection
  - Improved password security documentation
  - Created password hash generation tool
  
- **v1.0** - Initial Release
  - Four-sensor monitoring
  - Web dashboard with interactive charts
  - CSV data logging
  - NTP time synchronization
  - Basic SHA256 authentication
  - Connection rate limiting

## Acknowledgments

- Built for Seegrid aging room environmental monitoring
- Uses Chart.js for web visualization
- NTP implementation based on Arduino examples
- Security features implement OWASP best practices
- Salted hashing follows industry-standard password storage guidelines