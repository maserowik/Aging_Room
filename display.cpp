#include "display.h"
#include "sensors.h"

// Global LCD object
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Display state
int displayMode = 0;
unsigned long lastDisplaySwitch = 0;

static void printTempReading(float value, float threshold, bool blink) {
  if (isnan(value)) {
    lcd.print(blink ? "ERR  " : "     ");
    return;
  }

  if (abs(value - threshold) > THRESHOLD_MARGIN && blink) {
    lcd.print("     ");
    return;
  }

  lcd.print(value, 1);
  lcd.print(" C");
}

static void printHumidityReading(float value, bool blink) {
  if (isnan(value)) {
    lcd.print(blink ? "ERR  " : "     ");
    return;
  }

  lcd.print(value, 1);
  lcd.print(" %");
}

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

void updateDisplay() {
  unsigned long now = millis();

  if (now - lastDisplaySwitch >= 10000) {
    displayMode = !displayMode;
    lcd.clear();
    lastDisplaySwitch = now;
  }

  lcd.setCursor(0, 0);
  lcd.print("Seegrid Aging Room");

  extern float tA, tB, tC, tD, hA, hB, hC, hD;
  extern float tempThreshold;
  extern bool blinkState;

  if (displayMode == 0) {
    lcd.setCursor(0, 1);
    lcd.print("Temperature       ");
    lcd.setCursor(0, 2);
    lcd.print("A: ");
    printTempReading(tA, tempThreshold, blinkState);
    lcd.setCursor(10, 2);
    lcd.print("B: ");
    printTempReading(tB, tempThreshold, blinkState);
    lcd.setCursor(0, 3);
    lcd.print("C: ");
    printTempReading(tC, tempThreshold, blinkState);
    lcd.setCursor(10, 3);
    lcd.print("D: ");
    printTempReading(tD, tempThreshold, blinkState);
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Humidity          ");
    lcd.setCursor(0, 2);
    lcd.print("A: ");
    printHumidityReading(hA, blinkState);
    lcd.setCursor(10, 2);
    lcd.print("B: ");
    printHumidityReading(hB, blinkState);
    lcd.setCursor(0, 3);
    lcd.print("C: ");
    printHumidityReading(hC, blinkState);
    lcd.setCursor(10, 3);
    lcd.print("D: ");
    printHumidityReading(hD, blinkState);
  }
}
