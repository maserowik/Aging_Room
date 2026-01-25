#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <DHT.h>

// External variables
extern DHT dhtA, dhtB, dhtC, dhtD;
extern float tA, tB, tC, tD;
extern float hA, hB, hC, hD;
extern float tempThreshold;
extern bool blinkState;
extern unsigned long lastBlinkToggle;
extern unsigned long lastSensorRead;

// Function prototypes
void initSensors();
void readSensors();
void updateLEDs();
void handleButtonPress();

#endif // SENSORS_H