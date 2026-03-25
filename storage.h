#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>

// Tell the Arduino that this timer variable exists in storage.cpp
extern unsigned long lastCsvWrite;

// Initialize the SD Card
void initSDCard();

// Create the CSV headers if the files don't exist
void createCsvHeaderIfNeeded();

// Write the current sensor data to the SD card
void appendCsvData();

// Delete files that are exactly 180 days old
void purgeOldLogs();

// Web server route handlers
void serveFile(EthernetClient &client, const char *filename, const char *contentType);
void serveThreshold(EthernetClient &client);
void serveStatus(EthernetClient &client);
void serveSystemInfo(EthernetClient &client);
void serveRootPage(EthernetClient &client);

// Measure free RAM
int freeMemory();

#endif