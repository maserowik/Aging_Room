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
#define MIN_THRESHOLD 20
#define MAX_THRESHOLD 50
#define DEFAULT_TEMP_THRESHOLD 20

// Connection Limiting
#define MAX_GLOBAL_CONNECTIONS 8
#define MAX_PER_IP_CONNECTIONS 3
#define CONNECTION_TRACKING_SIZE 15
#define CONNECTION_TIMEOUT 300000

// DHT Sensor Configuration
#define DHTTYPE DHT22

// Pin Definitions
#define RED_LED_PIN 8
#define GREEN_LED_PIN 7
#define BUTTON_PIN 13

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
#define AUTH_PASSWORD_SHA256 "8b3d7f4a1c2e9f6b5a8d3c1e4f7a9b2c5d8e1f4a7b0c3d6e9f2a5b8c1d4e7f0a"

// Temperature Threshold
#define THRESHOLD_MARGIN 3.0

#endif // CONFIG_H