#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include "network.h"   // Required for epochToDateTime()

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

// Measure free RAM
int freeMemory();

// --- Aging Room web server route handlers ---
void serveFile(EthernetClient &client, const char *filename, const char *contentType);
void serveThreshold(EthernetClient &client);
void serveStatus(EthernetClient &client);
void serveSystemInfo(EthernetClient &client);
void serveRootPage(EthernetClient &client);

// --- Skit Room web server route handlers ---
void serveSkitPage(EthernetClient &client);
void serveSkitStatus(EthernetClient &client);
void serveSkitSysInfo(EthernetClient &client);
void serveSkitThresholdTemp(EthernetClient &client);
void serveSkitThresholdHumid(EthernetClient &client);

// --- Camera Room web server route handlers ---
void serveCameraPage(EthernetClient &client);
void serveCameraStatus(EthernetClient &client);
void serveCameraSysInfo(EthernetClient &client);
void serveCameraThresholdTemp(EthernetClient &client);
void serveCameraThresholdHumid(EthernetClient &client);

// --- Admin page (hidden — no nav link) ---
void serveAdminPage(EthernetClient &client);
void handleAdminDelete(EthernetClient &client, const char *filename);

#endif
