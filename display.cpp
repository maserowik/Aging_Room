#include "display.h"
#include "sensors.h"

// Global LCD object
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Display state — 0-5 cycles through all three rooms
// 0 = Aging Room Temperature
// 1 = Aging Room Humidity
// 2 = Skit Room Temperature + Humidity
// 3 = Skit Room Threshold Status
// 4 = Camera Room Temperature + Humidity
// 5 = Camera Room Threshold Status
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

// Helper — prints a float value or ERR/-- depending on state
// col/row = cursor position, width = total field width to pad/clear
static void printSensorVal(float val, int col, int row, const char* unit) {
  lcd.setCursor(col, row);
  if (isnan(val)) {
    lcd.print("ERR  ");
  } else {
    // Print value with 1 decimal place followed by unit
    char buf[8];
    dtostrf(val, 4, 1, buf);
    lcd.print(buf);
    lcd.print(unit);
  }
}

// Helper — prints threshold status: OK, LOW, HIGH, or ERR
static void printThreshStatus(float val, float threshold, float margin, int col, int row) {
  lcd.setCursor(col, row);
  if (isnan(val)) {
    lcd.print("ERR  ");
  } else if (val < threshold - margin) {
    lcd.print("LOW  ");
  } else if (val > threshold + margin) {
    lcd.print("HIGH ");
  } else {
    lcd.print("OK   ");
  }
}

void updateDisplay() {
  unsigned long now = millis();

  if (now - lastDisplaySwitch >= 10000) {
    displayMode = (displayMode + 1) % 6;
    lcd.clear();
    lastDisplaySwitch = now;
  }

  extern float tempThreshold;
  extern float skitTempThreshold, skitHumidThreshold;
  extern float camTempThreshold,  camHumidThreshold;
  extern bool blinkState;

  switch (displayMode) {

    // --------------------------------------------------------
    case 0:  // Aging Room — Temperature
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
    case 1:  // Aging Room — Humidity
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
    case 2:  // Skit Room — Temperature + Humidity
      lcd.setCursor(0, 0);
      lcd.print("Skit Room           ");
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      printSensorVal(tSkit, 6, 1, "C  ");
      lcd.setCursor(0, 2);
      lcd.print("Humid:");
      printSensorVal(hSkit, 6, 2, "%  ");
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      break;

    // --------------------------------------------------------
    case 3:  // Skit Room — Threshold Status
      lcd.setCursor(0, 0);
      lcd.print("Skit Threshold      ");
      lcd.setCursor(0, 1);
      lcd.print("T:");
      lcd.print((int)skitTempThreshold);
      lcd.print("C St:");
      printThreshStatus(tSkit, skitTempThreshold, THRESHOLD_MARGIN, 9, 1);
      lcd.setCursor(0, 2);
      lcd.print("H:");
      lcd.print((int)skitHumidThreshold);
      lcd.print("% St:");
      printThreshStatus(hSkit, skitHumidThreshold, THRESHOLD_MARGIN, 9, 2);
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      break;

    // --------------------------------------------------------
    case 4:  // Camera Room — Temperature + Humidity
      lcd.setCursor(0, 0);
      lcd.print("Camera Room         ");
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      printSensorVal(tCam, 6, 1, "C  ");
      lcd.setCursor(0, 2);
      lcd.print("Humid:");
      printSensorVal(hCam, 6, 2, "%  ");
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      break;

    // --------------------------------------------------------
    case 5:  // Camera Room — Threshold Status
      lcd.setCursor(0, 0);
      lcd.print("Camera Threshold    ");
      lcd.setCursor(0, 1);
      lcd.print("T:");
      lcd.print((int)camTempThreshold);
      lcd.print("C St:");
      printThreshStatus(tCam, camTempThreshold, THRESHOLD_MARGIN, 9, 1);
      lcd.setCursor(0, 2);
      lcd.print("H:");
      lcd.print((int)camHumidThreshold);
      lcd.print("% St:");
      printThreshStatus(hCam, camHumidThreshold, THRESHOLD_MARGIN, 9, 2);
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      break;
  }
}
