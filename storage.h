#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include <SD.h>
#include <Ethernet.h>

// External variables
extern unsigned long lastCsvWrite;

// Function prototypes
void initSDCard();
void createCsvHeaderIfNeeded();
void appendCsvData();
void serveFile(EthernetClient &client, const char *filename, const char *contentType);
void serveThreshold(EthernetClient &client);
void serveRootPage(EthernetClient &client);

#endif // STORAGE_H
