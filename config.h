#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SD.h>
#include <Crypto.h>
#include <SHA256.h>
// Note: base64.hpp is included only in auth.cpp to avoid multiple definition errors

// EEPROM Configuration
#define EEPROM_TEMP_THRESHOLD_ADDR       0   // 4 bytes — Aging Room temp threshold
#define EEPROM_SKIT_TEMP_THRESHOLD_ADDR  4   // 4 bytes — Skit temp threshold
#define EEPROM_SKIT_HUMID_THRESHOLD_ADDR 8   // 4 bytes — Skit humidity threshold
#define EEPROM_CAM_TEMP_THRESHOLD_ADDR   12  // 4 bytes — Camera temp threshold
#define EEPROM_CAM_HUMID_THRESHOLD_ADDR  16  // 4 bytes — Camera humidity threshold

// Threshold Configuration — Aging Room
#define MIN_THRESHOLD          -40
#define MAX_THRESHOLD          80
#define DEFAULT_TEMP_THRESHOLD 42

// Threshold Configuration — Skit and Camera
#define DEFAULT_SKIT_TEMP_THRESHOLD   22.0
#define DEFAULT_SKIT_HUMID_THRESHOLD  50.0
#define DEFAULT_CAM_TEMP_THRESHOLD    22.0
#define DEFAULT_CAM_HUMID_THRESHOLD   50.0
#define MIN_HUMID_THRESHOLD           0.0
#define MAX_HUMID_THRESHOLD           100.0

// Connection Limiting
#define MAX_GLOBAL_CONNECTIONS   8
#define MAX_PER_IP_CONNECTIONS   3
#define CONNECTION_TRACKING_SIZE 8
#define CONNECTION_TIMEOUT       300000

// DHT Sensor Configuration
#define DHTTYPE DHT22

// Pin Definitions — Aging Room
#define RED_LED_PIN   46
#define GREEN_LED_PIN 47
#define BUTTON_PIN    50

// Pin Definitions — RS485 (Mega Serial1: RX=19, TX=18, DE+RE tied to GND)
#define RS485_BAUD    9600

// Timing Constants
#define BLINK_INTERVAL_NORMAL 500
#define BLINK_INTERVAL_FAST   250
#define SENSOR_READ_INTERVAL  2000
#define CSV_WRITE_INTERVAL    300000
// NTP syncs at 00:00 and 12:00 every calendar day — see Aging_Room.ino loop()

// RS485 receive timeout — if no packet within this window sensor is marked stale
// Covers 6-min (Skit) and 7-min (Camera) transmit intervals with margin
#define RS485_TIMEOUT_MS  600000UL  // 10 minutes

// SD Card Configuration
#define SD_CHIP_SELECT 4

// Network Configuration
#define SERVER_PORT    80
#define UDP_LOCAL_PORT 5203
#define NTP_PACKET_SIZE 48

// Authentication
#define AUTH_USERNAME "Seegrid"
//#define AUTH_SALT "SeegridAgingRoom2026"  // CHANGE THIS to your unique salt
#define AUTH_SALT "216_Aging_Room"
//#define AUTH_PASSWORD_SHA256 "8b3d7f4a1c2e9f6b5a8d3c1e4f7a9b2c5d8e1f4a7b0c3d6e9f2a5b8c1d4e7f0a"
#define AUTH_PASSWORD_SHA256 "73c7fb3c9a3521a178e61bba9009b21179e95bf9ab8b3c891fc5036bc9f490c8"

// Temperature Threshold
#define THRESHOLD_MARGIN 5.0

#endif // CONFIG_H
