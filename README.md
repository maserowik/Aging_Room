# Seegrid Aging Room Environmental Monitoring System

Arduino-based temperature and humidity monitoring system with web interface, data logging, and real-time alerts for industrial aging room environments.

## Features

- **Four DHT22 Sensors**: Simultaneous monitoring of temperature and humidity across multiple zones
- **Real-time LCD Display**: 20x4 I2C LCD showing live sensor readings with automatic screen rotation
- **Web Dashboard**: Interactive charts with configurable time ranges (1, 3, 5, 7 days)
- **CSV Data Logging**: Automatic timestamped data capture every 5 minutes to SD card
- **Visual Alerts**: Red/Green LED indicators with distinct blink patterns for error states
- **NTP Time Sync**: Automatic network time synchronization with DST support
- **Security**: SHA256 password authentication and connection rate limiting
- **Adjustable Threshold**: Physical button interface for on-device temperature threshold configuration

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

| Component      | Pin | Notes        |
|----------------|-----|--------------|
| DHT22 Sensor A | 2   | Digital      |
| DHT22 Sensor B | 3   | Digital      |
| DHT22 Sensor C | 5   | Digital      |
| DHT22 Sensor D | 6   | Digital      |
| Green LED      | 7   | Digital      |
| Red LED        | 8   | Digital      |
| Ethernet CS    | 10  | SPI          |
| Button         | 13  | INPUT_PULLUP |
| SD Card CS     | 4   | SPI          |
| LCD            | I2C | SDA/SCL      |

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
git clone https://gitlab.com/yourusername/aging_room.git
cd aging_room
```

### 3. Configure Network

Edit `Aging_Room.ino` lines 31-34 if you need to change the static IP fallback:

```cpp
IPAddress ip(192, 168, 16, 70);      // Static IP
IPAddress gateway(192, 168, 16, 1);   // Gateway
IPAddress subnet(255, 255, 255, 0);   // Subnet mask
IPAddress dns(192, 168, 16, 1);       // DNS server
```

### 4. Set Authentication Password

**Default Credentials:**
- Username: `Seegrid`
- Password: **You must set this** (see below)

**To set a new password:**

1. Visit https://emn178.github.io/online-tools/sha256.html
2. Enter your desired password in the input field
3. Copy the resulting 64-character hex hash
4. Edit `config.h` line 56:
   ```cpp
   #define AUTH_PASSWORD_SHA256 "your_64_character_hash_here"
   ```
5. Save the file and upload to your Arduino

**Example:**
- If your password is: `MySecurePass123`
- The SHA256 hash would be: `a1b2c3d4e5f6...` (64 characters)
- Replace the entire hash in `config.h`

**Important Security Notes:**
- Never commit your actual password to the repository
- Use a strong password (mix of letters, numbers, symbols)
- Keep the hash private if possible
- The hash is lowercase hexadecimal only

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
├── auth.cpp             # SHA256 password validation logic
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

### LED Indicator Meanings

| Pattern                | Meaning                               |
|---------               |---------                              |
| Solid Green            | All sensors within threshold ±3°C     |
| Slow Red Blink (500ms) | One or more sensors outside threshold |
| Fast Red Blink (250ms) | Sensor error or read failure          |

## Security Features

### Authentication
- **SHA256 Password Hashing**: Passwords never stored in plaintext
- **HTTP Basic Authentication**: All endpoints protected (root page, CSV downloads, file deletion)

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

### Additional Security
- **Request Size Limiting**: 512 byte maximum request size prevents buffer overflow
- **Request Timeout**: 5 second timeout per request prevents slowloris attacks

## Data Logging

- **Interval**: Every 5 minutes (300,000ms)
- **Files**: `temp.csv` and `humid.csv` on SD card
- **Format**: Date, Time, Sensor A, Sensor B, Sensor C, Sensor D
- **Time Sync**: NTP updates every 24 hours from time.nist.gov
- **Timezone**: Eastern Time (EST/EDT with automatic DST)

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

### Time Not Syncing
- Check internet connectivity
- Verify NTP server is accessible (129.6.15.28)
- Check Serial Monitor for NTP response messages

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

- **v1.0** - Initial release with modular file structure
  - Four-sensor monitoring
  - Web dashboard with interactive charts
  - CSV data logging
  - NTP time synchronization
  - SHA256 authentication
  - Connection rate limiting

## Acknowledgments

- Built for Seegrid aging room environmental monitoring
- Uses Chart.js for web visualization
- NTP implementation based on Arduino examples
- Security features implement industry best practices