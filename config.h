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
#define EEPROM_TEMP_THRESHOLD_ADDR 0

// Threshold Configuration
#define MIN_THRESHOLD          -40
#define MAX_THRESHOLD          80
#define DEFAULT_TEMP_THRESHOLD 42

// Connection Limiting
#define MAX_GLOBAL_CONNECTIONS 8
#define MAX_PER_IP_CONNECTIONS 3
#define CONNECTION_TRACKING_SIZE 15
#define CONNECTION_TIMEOUT 300000

// DHT Sensor Configuration
#define DHTTYPE DHT22

// Pin Definitions
#define RED_LED_PIN 46
#define GREEN_LED_PIN 47
#define BUTTON_PIN 50

// Timing Constants
#define BLINK_INTERVAL_NORMAL 500
#define BLINK_INTERVAL_FAST 250
#define SENSOR_READ_INTERVAL 2000
#define CSV_WRITE_INTERVAL 300000
#define NTP_INTERVAL 86400000

// SD Card Configuration
#define SD_CHIP_SELECT 4

// Network Configuration
#define SERVER_PORT 80
#define UDP_LOCAL_PORT 5203
#define NTP_PACKET_SIZE 48

// Authentication
#define AUTH_USERNAME "Seegrid"
#define AUTH_SALT "216_Aging_Room"
#define AUTH_PASSWORD_SHA256 "73c7fb3c9a3521a178e61bba9009b21179e95bf9ab8b3c891fc5036bc9f490c8"

// Temperature Threshold
#define THRESHOLD_MARGIN 5.0

// Sentinel value: epoch values above this indicate a valid NTP sync has occurred.
// Unix timestamp 1,000,000,000 = September 9, 2001 — safely before any real deployment date.
#define EPOCH_VALID_THRESHOLD 1000000000UL

#endif // CONFIG_H
