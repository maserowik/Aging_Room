#include "display.h"
#include "sensors.h"

// Global LCD object
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Display state — 0-3 cycles through all three rooms
// 0 = Aging Room Temperature
// 1 = Aging Room Humidity
// 2 = Skit Room Temperature + Humidity + Threshold Status
// 3 = Camera Room Temperature + Humidity + Threshold Status
int displayMode = 0;
unsigned long lastDisplaySwitch = 0;

void initDisplay() {
  lcd.init();
  lcd.backlight();
}

void bootSequence() {
  for (int i = 0; i < 5; i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Booting");
    for (int dot = 0; dot <= i && dot < 3; dot++) lcd.print(".");
    delay(1000);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Red/Green Stack");
  lcd.setCursor(0, 1);
  lcd.print("LED Testing");
  for (int i = 0; i < 2; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(250);
    digitalWrite(RED_LED_PIN, LOW);
    delay(250);
  }
  for (int i = 0; i < 2; i++) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(250);
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(250);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Testing");
  delay(1000);
  lcd.clear();
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 20; col++) {
      lcd.setCursor(col, row);
      lcd.write(255);
      delay(125);
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Getting Ready");
  lcd.setCursor(0, 2);
  lcd.print("Standby");
  delay(3000);
  lcd.clear();
}

// ============================================================
// printRoomVal — prints a sensor value with blink-on-out-of-range
// val       : sensor reading (NAN = error)
// threshold : the threshold for this value type
// col/row   : LCD cursor position
// unit      : unit string e.g. "C" or "%"
// blinkState: current blink state from updateLEDs()
// ============================================================
static void printRoomVal(float val, float threshold, int col, int row,
                         const char* unit, bool blinkState) {
  lcd.setCursor(col, row);
  if (isnan(val)) {
    lcd.print(blinkState ? "ERR  " : "     ");
  } else if (abs(val - threshold) > THRESHOLD_MARGIN && blinkState) {
    lcd.print("     ");
  } else {
    char buf[7];
    dtostrf(val, 4, 1, buf);
    lcd.print(buf);
    lcd.print(unit);
  }
}

// ============================================================
// printRoomStatus — prints OK / LOW / HIGH / ERR (5 chars)
// Blinks blank when out of range
// ============================================================
static void printRoomStatus(float val, float threshold, int col, int row, bool blinkState) {
  lcd.setCursor(col, row);
  if (isnan(val)) {
    lcd.print(blinkState ? "ERR  " : "     ");
  } else if (val < threshold - THRESHOLD_MARGIN) {
    lcd.print(blinkState ? "     " : "LOW  ");
  } else if (val > threshold + THRESHOLD_MARGIN) {
    lcd.print(blinkState ? "     " : "HIGH ");
  } else {
    lcd.print("OK   ");
  }
}

void updateDisplay() {
  unsigned long now = millis();

  if (now - lastDisplaySwitch >= 10000) {
    displayMode = (displayMode + 1) % 4;
    lcd.clear();
    lastDisplaySwitch = now;
  }

  extern float tempThreshold;
  extern float skitTempThreshold, skitHumidThreshold;
  extern float camTempThreshold,  camHumidThreshold;
  extern bool blinkState;

  switch (displayMode) {

    // --------------------------------------------------------
    // Screen 0 — Aging Room Temperature
    // Row 0: "Aging Room Temp     "
    // Row 1: "A:XX.XC  B:XX.XC   "
    // Row 2: "C:XX.XC  D:XX.XC   "
    // Row 3: "Thresh: XXC         "
    // Out-of-range values blink blank. ERR blinks ERR/blank.
    // --------------------------------------------------------
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Aging Room Temp     ");
      lcd.setCursor(0, 1);
      lcd.print("A:");
      { char b[7]; if(isnan(tA)){lcd.print(blinkState?"ERR  ":"     ");} else if(abs(tA-tempThreshold)>THRESHOLD_MARGIN&&blinkState){lcd.print("     ");} else{dtostrf(tA,4,1,b);lcd.print(b);lcd.print("C");} }
      lcd.setCursor(10, 1);
      lcd.print("B:");
      { char b[7]; if(isnan(tB)){lcd.print(blinkState?"ERR  ":"     ");} else if(abs(tB-tempThreshold)>THRESHOLD_MARGIN&&blinkState){lcd.print("     ");} else{dtostrf(tB,4,1,b);lcd.print(b);lcd.print("C");} }
      lcd.setCursor(0, 2);
      lcd.print("C:");
      { char b[7]; if(isnan(tC)){lcd.print(blinkState?"ERR  ":"     ");} else if(abs(tC-tempThreshold)>THRESHOLD_MARGIN&&blinkState){lcd.print("     ");} else{dtostrf(tC,4,1,b);lcd.print(b);lcd.print("C");} }
      lcd.setCursor(10, 2);
      lcd.print("D:");
      { char b[7]; if(isnan(tD)){lcd.print(blinkState?"ERR  ":"     ");} else if(abs(tD-tempThreshold)>THRESHOLD_MARGIN&&blinkState){lcd.print("     ");} else{dtostrf(tD,4,1,b);lcd.print(b);lcd.print("C");} }
      lcd.setCursor(0, 3);
      lcd.print("Thresh:");
      lcd.print((int)tempThreshold);
      lcd.print("C           ");
      break;

    // --------------------------------------------------------
    // Screen 1 — Aging Room Humidity
    // Row 0: "Aging Room Humidity "
    // Row 1: "A:XX.X%  B:XX.X%   "
    // Row 2: "C:XX.X%  D:XX.X%   "
    // Row 3: blank
    // --------------------------------------------------------
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Aging Room Humidity ");
      lcd.setCursor(0, 1);
      lcd.print("A:");
      { char b[7]; if(isnan(hA)){lcd.print(blinkState?"ERR  ":"     ");} else{dtostrf(hA,4,1,b);lcd.print(b);lcd.print("%");} }
      lcd.setCursor(10, 1);
      lcd.print("B:");
      { char b[7]; if(isnan(hB)){lcd.print(blinkState?"ERR  ":"     ");} else{dtostrf(hB,4,1,b);lcd.print(b);lcd.print("%");} }
      lcd.setCursor(0, 2);
      lcd.print("C:");
      { char b[7]; if(isnan(hC)){lcd.print(blinkState?"ERR  ":"     ");} else{dtostrf(hC,4,1,b);lcd.print(b);lcd.print("%");} }
      lcd.setCursor(10, 2);
      lcd.print("D:");
      { char b[7]; if(isnan(hD)){lcd.print(blinkState?"ERR  ":"     ");} else{dtostrf(hD,4,1,b);lcd.print(b);lcd.print("%");} }
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      break;

    // --------------------------------------------------------
    // Screen 2 — Skit Room (collapsed)
    // Row 0: "Skit Room           "
    // Row 1: "Temp: XX.XC  OK     "   (blinks blank when out of range)
    // Row 2: "Humid:XX.X%  OK     "   (blinks blank when out of range)
    // Row 3: "T:XXC   H:XX%       "   (threshold reference)
    // --------------------------------------------------------
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Skit Room           ");

      // Row 1 — Temperature value + status
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      printRoomVal(tSkit, skitTempThreshold, 6, 1, "C  ", blinkState);
      printRoomStatus(tSkit, skitTempThreshold, 13, 1, blinkState);

      // Row 2 — Humidity value + status
      lcd.setCursor(0, 2);
      lcd.print("Humid:");
      printRoomVal(hSkit, skitHumidThreshold, 6, 2, "%  ", blinkState);
      printRoomStatus(hSkit, skitHumidThreshold, 13, 2, blinkState);

      // Row 3 — Threshold reference (static, no blink needed)
      lcd.setCursor(0, 3);
      lcd.print("T:");
      lcd.print((int)skitTempThreshold);
      lcd.print("C  H:");
      lcd.print((int)skitHumidThreshold);
      lcd.print("%        ");
      break;

    // --------------------------------------------------------
    // Screen 3 — Camera Room (collapsed)
    // Row 0: "Camera Room         "
    // Row 1: "Temp: XX.XC  OK     "   (blinks blank when out of range)
    // Row 2: "Humid:XX.X%  OK     "   (blinks blank when out of range)
    // Row 3: "T:XXC   H:XX%       "   (threshold reference)
    // --------------------------------------------------------
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Camera Room         ");

      // Row 1 — Temperature value + status
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      printRoomVal(tCam, camTempThreshold, 6, 1, "C  ", blinkState);
      printRoomStatus(tCam, camTempThreshold, 13, 1, blinkState);

      // Row 2 — Humidity value + status
      lcd.setCursor(0, 2);
      lcd.print("Humid:");
      printRoomVal(hCam, camHumidThreshold, 6, 2, "%  ", blinkState);
      printRoomStatus(hCam, camHumidThreshold, 13, 2, blinkState);

      // Row 3 — Threshold reference (static, no blink needed)
      lcd.setCursor(0, 3);
      lcd.print("T:");
      lcd.print((int)camTempThreshold);
      lcd.print("C  H:");
      lcd.print((int)camHumidThreshold);
      lcd.print("%        ");
      break;
  }
}