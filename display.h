#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include <LiquidCrystal_I2C.h>

// External variables
extern LiquidCrystal_I2C lcd;
extern int displayMode;           // 0-5: cycles through all three rooms
extern unsigned long lastDisplaySwitch;

// Function prototypes
void initDisplay();
void updateDisplay();
void bootSequence();

#endif // DISPLAY_H
