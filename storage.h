#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>

// Tracks time of last successful CSV write (set in appendCsvData)
extern unsigned long lastCsvWrite;

// Initialize the SD card; halts with error blink if it fails
void initSDCard();

// Write the current sensor readings to today's daily CSV files
void appendCsvData();

// Delete log files older than 180 days (scans a 20-day window to catch gaps)
void purgeOldLogs();

// Web server route handlers
void serveFile(EthernetClient &client, const char *filename, const char *contentType);
void serveThreshold(EthernetClient &client);
void serveStatus(EthernetClient &client);
void serveSystemInfo(EthernetClient &client);
void serveRootPage(EthernetClient &client);

// Returns free heap+stack space in bytes
int freeMemory();

#endif
