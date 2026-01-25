#include "sensors.h"
#include "display.h"

// Global sensor objects
DHT dhtA(2, DHTTYPE);
DHT dhtB(3, DHTTYPE);
DHT dhtC(5, DHTTYPE);
DHT dhtD(6, DHTTYPE);

// Sensor data
float tA = NAN, tB = NAN, tC = NAN, tD = NAN;
float hA = NAN, hB = NAN, hC = NAN, hD = NAN;
float tempThreshold = DEFAULT_TEMP_THRESHOLD;

// Timing variables
bool blinkState = false;
unsigned long lastBlinkToggle = 0;
unsigned long lastSensorRead = 0;

void initSensors() {
  dhtA.begin();
  dhtB.begin();
  dhtC.begin();
  dhtD.begin();
  
  EEPROM.get(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  if (tempThreshold < MIN_THRESHOLD || tempThreshold > MAX_THRESHOLD) {
    tempThreshold = DEFAULT_TEMP_THRESHOLD;
  }
}

void readSensors() {
  unsigned long now = millis();
  
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    tA = dhtA.readTemperature();
    if (isnan(tA)) {
      delay(500);
      tA = dhtA.readTemperature();
    }
    tB = dhtB.readTemperature();
    if (isnan(tB)) {
      delay(500);
      tB = dhtB.readTemperature();
    }
    tC = dhtC.readTemperature();
    if (isnan(tC)) {
      delay(500);
      tC = dhtC.readTemperature();
    }
    tD = dhtD.readTemperature();
    if (isnan(tD)) {
      delay(500);
      tD = dhtD.readTemperature();
    }

    hA = dhtA.readHumidity();
    if (isnan(hA)) {
      delay(500);
      hA = dhtA.readHumidity();
    }
    hB = dhtB.readHumidity();
    if (isnan(hB)) {
      delay(500);
      hB = dhtB.readHumidity();
    }
    hC = dhtC.readHumidity();
    if (isnan(hC)) {
      delay(500);
      hC = dhtC.readHumidity();
    }
    hD = dhtD.readHumidity();
    if (isnan(hD)) {
      delay(500);
      hD = dhtD.readHumidity();
    }

    lastSensorRead = now;
  }
}

void updateLEDs() {
  unsigned long now = millis();
  
  bool tempError = isnan(tA) || isnan(tB) || isnan(tC) || isnan(tD);
  bool tempOutOfRange =
    (!isnan(tA) && abs(tA - tempThreshold) > THRESHOLD_MARGIN) || 
    (!isnan(tB) && abs(tB - tempThreshold) > THRESHOLD_MARGIN) || 
    (!isnan(tC) && abs(tC - tempThreshold) > THRESHOLD_MARGIN) || 
    (!isnan(tD) && abs(tD - tempThreshold) > THRESHOLD_MARGIN);

  unsigned long blinkInterval;

  if (tempError) {
    blinkInterval = BLINK_INTERVAL_FAST;
  } else if (tempOutOfRange) {
    blinkInterval = BLINK_INTERVAL_NORMAL;
  } else {
    blinkInterval = 0;
  }

  if (blinkInterval > 0 && now - lastBlinkToggle >= blinkInterval) {
    blinkState = !blinkState;
    lastBlinkToggle = now;
  }

  if (tempError) {
    digitalWrite(RED_LED_PIN, blinkState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else if (tempOutOfRange) {
    digitalWrite(RED_LED_PIN, blinkState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  }
}

void handleButtonPress() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long holdStart = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      digitalWrite(RED_LED_PIN, (millis() / 250) % 2);
      digitalWrite(GREEN_LED_PIN, !((millis() / 250) % 2));
      if (millis() - holdStart >= 5000) break;
      delay(50);
    }

    if (millis() - holdStart >= 5000) {
      float oldThreshold = tempThreshold;
      unsigned long lastIncTime = 0;
      extern LiquidCrystal_I2C lcd;
      lcd.clear();

      for (int i = 0; i < 10; i++) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
        delay(250);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(250);
      }

      while (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - lastBlinkToggle >= 250) {
          blinkState = !blinkState;
          lastBlinkToggle = millis();
        }

        digitalWrite(GREEN_LED_PIN, blinkState ? HIGH : LOW);
        digitalWrite(RED_LED_PIN, LOW);

        if (millis() - lastIncTime >= 2000) {
          tempThreshold += 1.0;
          if (tempThreshold > 50.0) tempThreshold = 20.0;
          lastIncTime = millis();
        }

        if (blinkState) {
          lcd.setCursor(0, 0);
          lcd.print("Adjustment Mode   ");
        } else {
          lcd.setCursor(0, 0);
          lcd.print("                  ");
        }

        lcd.setCursor(0, 1);
        lcd.print("Adjusting...      ");
        lcd.setCursor(0, 2);
        lcd.print("New Threshold: ");
        lcd.print((int)tempThreshold);
        lcd.setCursor(0, 3);
        lcd.print("Release to exit   ");
        delay(50);
      }

      EEPROM.put(0, tempThreshold);

      for (int i = 0; i < 10; i++) {
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(250);
        digitalWrite(RED_LED_PIN, LOW);
        delay(250);
      }

      lcd.clear();
      for (int i = 0; i < 20; i++) {
        lcd.setCursor(0, 0);
        lcd.print("Threshold Updated ");
        lcd.setCursor(0, 2);
        lcd.print("Old: ");
        lcd.print((int)oldThreshold);
        lcd.setCursor(0, 3);
        lcd.print("New: ");
        lcd.print((int)tempThreshold);

        digitalWrite(RED_LED_PIN, i % 2);
        digitalWrite(GREEN_LED_PIN, i % 2);
        delay(500);
      }

      lcd.clear();
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }
}