#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"
#include <DHT.h>

// --- Aging Room sensor objects and data ---
extern DHT dhtA, dhtB, dhtC, dhtD;
extern float tA, tB, tC, tD;
extern float hA, hB, hC, hD;
extern float tempThreshold;

// --- Skit Room sensor data (received via RS485) ---
extern float tSkit;
extern float hSkit;
extern float skitTempThreshold;
extern float skitHumidThreshold;
extern unsigned long lastSkitReceive;  // millis() of last valid packet

// --- Camera Room sensor data (received via RS485) ---
extern float tCam;
extern float hCam;
extern float camTempThreshold;
extern float camHumidThreshold;
extern unsigned long lastCamReceive;   // millis() of last valid packet

// --- LED / blink state ---
extern bool blinkState;
extern unsigned long lastBlinkToggle;
extern unsigned long lastSensorRead;

// --- Function prototypes ---
void initSensors();
void readSensors();
void readRS485();
void updateLEDs();
void handleButtonPress();

#endif // SENSORS_H
