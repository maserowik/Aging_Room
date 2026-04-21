#include "sensors.h"
#include "display.h"
#include <avr/wdt.h>

// --- Aging Room sensor objects ---
DHT dhtA(40, DHTTYPE);
DHT dhtB(41, DHTTYPE);
DHT dhtC(30, DHTTYPE);
DHT dhtD(31, DHTTYPE);

// --- Aging Room sensor data ---
float tA = NAN, tB = NAN, tC = NAN, tD = NAN;
float hA = NAN, hB = NAN, hC = NAN, hD = NAN;
float tempThreshold = DEFAULT_TEMP_THRESHOLD;

// --- Skit Room sensor data ---
float tSkit = NAN;
float hSkit = NAN;
float skitTempThreshold  = DEFAULT_SKIT_TEMP_THRESHOLD;
float skitHumidThreshold = DEFAULT_SKIT_HUMID_THRESHOLD;
unsigned long lastSkitReceive = 0;

// --- Camera Room sensor data ---
float tCam = NAN;
float hCam = NAN;
float camTempThreshold  = DEFAULT_CAM_TEMP_THRESHOLD;
float camHumidThreshold = DEFAULT_CAM_HUMID_THRESHOLD;
unsigned long lastCamReceive = 0;

// --- LED / blink state ---
bool blinkState = false;
unsigned long lastBlinkToggle = 0;
unsigned long lastSensorRead  = 0;

// --- RS485 receive buffer ---
static char rs485Buf[32];
static uint8_t rs485BufLen = 0;

// ============================================================
void initSensors() {
  dhtA.begin();
  dhtB.begin();
  dhtC.begin();
  dhtD.begin();

  // Aging Room temp threshold
  EEPROM.get(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  if (isnan(tempThreshold) || tempThreshold < MIN_THRESHOLD || tempThreshold > MAX_THRESHOLD) {
    tempThreshold = DEFAULT_TEMP_THRESHOLD;
    EEPROM.put(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  }

  // Skit temp threshold
  EEPROM.get(EEPROM_SKIT_TEMP_THRESHOLD_ADDR, skitTempThreshold);
  if (isnan(skitTempThreshold) || skitTempThreshold < MIN_THRESHOLD || skitTempThreshold > MAX_THRESHOLD) {
    skitTempThreshold = DEFAULT_SKIT_TEMP_THRESHOLD;
    EEPROM.put(EEPROM_SKIT_TEMP_THRESHOLD_ADDR, skitTempThreshold);
  }

  // Skit humidity threshold
  EEPROM.get(EEPROM_SKIT_HUMID_THRESHOLD_ADDR, skitHumidThreshold);
  if (isnan(skitHumidThreshold) || skitHumidThreshold < MIN_HUMID_THRESHOLD || skitHumidThreshold > MAX_HUMID_THRESHOLD) {
    skitHumidThreshold = DEFAULT_SKIT_HUMID_THRESHOLD;
    EEPROM.put(EEPROM_SKIT_HUMID_THRESHOLD_ADDR, skitHumidThreshold);
  }

  // Camera temp threshold
  EEPROM.get(EEPROM_CAM_TEMP_THRESHOLD_ADDR, camTempThreshold);
  if (isnan(camTempThreshold) || camTempThreshold < MIN_THRESHOLD || camTempThreshold > MAX_THRESHOLD) {
    camTempThreshold = DEFAULT_CAM_TEMP_THRESHOLD;
    EEPROM.put(EEPROM_CAM_TEMP_THRESHOLD_ADDR, camTempThreshold);
  }

  // Camera humidity threshold
  EEPROM.get(EEPROM_CAM_HUMID_THRESHOLD_ADDR, camHumidThreshold);
  if (isnan(camHumidThreshold) || camHumidThreshold < MIN_HUMID_THRESHOLD || camHumidThreshold > MAX_HUMID_THRESHOLD) {
    camHumidThreshold = DEFAULT_CAM_HUMID_THRESHOLD;
    EEPROM.put(EEPROM_CAM_HUMID_THRESHOLD_ADDR, camHumidThreshold);
  }

  // Start RS485 serial port
  Serial1.begin(RS485_BAUD);

  Serial.print(F("Aging Room threshold: "));  Serial.println(tempThreshold);
  Serial.print(F("Skit temp threshold: "));   Serial.println(skitTempThreshold);
  Serial.print(F("Skit humid threshold: "));  Serial.println(skitHumidThreshold);
  Serial.print(F("Camera temp threshold: ")); Serial.println(camTempThreshold);
  Serial.print(F("Camera humid threshold: ")); Serial.println(camHumidThreshold);
}

// ============================================================
void readSensors() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    tA = dhtA.readTemperature();
    if (isnan(tA)) { delay(100); tA = dhtA.readTemperature(); }

    tB = dhtB.readTemperature();
    if (isnan(tB)) { delay(100); tB = dhtB.readTemperature(); }

    tC = dhtC.readTemperature();
    if (isnan(tC)) { delay(100); tC = dhtC.readTemperature(); }

    tD = dhtD.readTemperature();
    if (isnan(tD)) { delay(100); tD = dhtD.readTemperature(); }

    hA = dhtA.readHumidity();
    if (isnan(hA)) { delay(100); hA = dhtA.readHumidity(); }

    hB = dhtB.readHumidity();
    if (isnan(hB)) { delay(100); hB = dhtB.readHumidity(); }

    hC = dhtC.readHumidity();
    if (isnan(hC)) { delay(100); hC = dhtC.readHumidity(); }

    hD = dhtD.readHumidity();
    if (isnan(hD)) { delay(100); hD = dhtD.readHumidity(); }

    lastSensorRead = now;
  }

  // Mark Skit and Camera sensors stale if no packet received within timeout
  if (lastSkitReceive > 0 && (now - lastSkitReceive > RS485_TIMEOUT_MS)) {
    tSkit = NAN;
    hSkit = NAN;
  }
  if (lastCamReceive > 0 && (now - lastCamReceive > RS485_TIMEOUT_MS)) {
    tCam = NAN;
    hCam = NAN;
  }
}

// ============================================================
// readRS485() — call every loop() iteration
// Reads incoming bytes from Serial1, builds lines terminated by '\n'
// Parses SKIT:temp,humid and CAM:temp,humid packets
// ============================================================
void readRS485() {
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '\n') {
      // Null-terminate and process the completed line
      rs485Buf[rs485BufLen] = '\0';

      // Determine room from prefix
      bool isSkit = (strncmp(rs485Buf, "SKIT:", 5) == 0);
      bool isCam  = (strncmp(rs485Buf, "CAM:",  4) == 0);

      if (isSkit || isCam) {
        const char* data = isSkit ? rs485Buf + 5 : rs485Buf + 4;

        // Find comma separating temp and humid
        const char* comma = strchr(data, ',');
        if (comma) {
          char tempBuf[8];
          char humidBuf[8];
          uint8_t tLen = comma - data;
          if (tLen < sizeof(tempBuf)) {
            memcpy(tempBuf, data, tLen);
            tempBuf[tLen] = '\0';
            strncpy(humidBuf, comma + 1, sizeof(humidBuf) - 1);
            humidBuf[sizeof(humidBuf) - 1] = '\0';

            // Check for ERR packet
            bool isErr = (strcmp(tempBuf, "ERR") == 0);

            if (isSkit) {
              tSkit = isErr ? NAN : atof(tempBuf);
              hSkit = isErr ? NAN : atof(humidBuf);
              lastSkitReceive = millis();
              Serial.print(F("Skit: "));
              Serial.print(tSkit); Serial.print(F("C, "));
              Serial.print(hSkit); Serial.println(F("%"));
            } else {
              tCam = isErr ? NAN : atof(tempBuf);
              hCam = isErr ? NAN : atof(humidBuf);
              lastCamReceive = millis();
              Serial.print(F("Camera: "));
              Serial.print(tCam); Serial.print(F("C, "));
              Serial.print(hCam); Serial.println(F("%"));
            }
          }
        }
      }

      // Reset buffer
      rs485BufLen = 0;

    } else if (c != '\r' && rs485BufLen < sizeof(rs485Buf) - 1) {
      rs485Buf[rs485BufLen++] = c;
    } else if (rs485BufLen >= sizeof(rs485Buf) - 1) {
      // Buffer overflow — discard and reset
      rs485BufLen = 0;
    }
  }
}

// ============================================================
void updateLEDs() {
  unsigned long now = millis();

  // Error = any sensor NAN across all three rooms
  bool anyError =
    isnan(tA) || isnan(tB) || isnan(tC) || isnan(tD) ||
    isnan(tSkit) || isnan(hSkit) ||
    isnan(tCam)  || isnan(hCam);

  // Out of range = any sensor outside threshold across all three rooms
  bool anyOutOfRange =
    (!isnan(tA) && abs(tA - tempThreshold) > THRESHOLD_MARGIN) ||
    (!isnan(tB) && abs(tB - tempThreshold) > THRESHOLD_MARGIN) ||
    (!isnan(tC) && abs(tC - tempThreshold) > THRESHOLD_MARGIN) ||
    (!isnan(tD) && abs(tD - tempThreshold) > THRESHOLD_MARGIN) ||
    (!isnan(tSkit) && abs(tSkit - skitTempThreshold)  > THRESHOLD_MARGIN) ||
    (!isnan(hSkit) && abs(hSkit - skitHumidThreshold) > THRESHOLD_MARGIN) ||
    (!isnan(tCam)  && abs(tCam  - camTempThreshold)   > THRESHOLD_MARGIN) ||
    (!isnan(hCam)  && abs(hCam  - camHumidThreshold)  > THRESHOLD_MARGIN);

  unsigned long blinkInterval;
  if (anyError)        blinkInterval = BLINK_INTERVAL_FAST;
  else if (anyOutOfRange) blinkInterval = BLINK_INTERVAL_NORMAL;
  else                 blinkInterval = 0;

  if (blinkInterval > 0 && now - lastBlinkToggle >= blinkInterval) {
    blinkState = !blinkState;
    lastBlinkToggle = now;
  }

  if (anyError) {
    digitalWrite(RED_LED_PIN,   blinkState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else if (anyOutOfRange) {
    digitalWrite(RED_LED_PIN,   blinkState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN,   LOW);
  }
}

// ============================================================
void handleButtonPress() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long holdStart = millis();

    while (digitalRead(BUTTON_PIN) == LOW) {
      wdt_reset();
      digitalWrite(RED_LED_PIN,   (millis() / 250) % 2);
      digitalWrite(GREEN_LED_PIN, !((millis() / 250) % 2));
      if (millis() - holdStart >= 5000) break;
      delay(50);
    }

    if (millis() - holdStart >= 5000) {
      float oldThreshold = tempThreshold;
      unsigned long lastIncTime = 0;

      lcd.clear();

      for (int i = 0; i < 10; i++) {
        wdt_reset();
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
        delay(250);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(250);
      }

      while (digitalRead(BUTTON_PIN) == LOW) {
        wdt_reset();

        if (millis() - lastBlinkToggle >= 250) {
          blinkState = !blinkState;
          lastBlinkToggle = millis();
        }

        digitalWrite(GREEN_LED_PIN, blinkState ? HIGH : LOW);
        digitalWrite(RED_LED_PIN, LOW);

        if (millis() - lastIncTime >= 2000) {
          tempThreshold += 1.0;
          if (tempThreshold > MAX_THRESHOLD) tempThreshold = MIN_THRESHOLD;
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
        lcd.print("  ");
        lcd.setCursor(0, 3);
        lcd.print("Release to save   ");

        delay(50);
      }

      EEPROM.put(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
      Serial.print(F("Aging Room threshold saved: "));
      Serial.println(tempThreshold);

      for (int i = 0; i < 10; i++) {
        wdt_reset();
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(250);
        digitalWrite(RED_LED_PIN, LOW);
        delay(250);
      }

      lcd.clear();
      for (int i = 0; i < 20; i++) {
        wdt_reset();
        lcd.setCursor(0, 0);
        lcd.print("Threshold Updated ");
        lcd.setCursor(0, 2);
        lcd.print("Old: ");
        lcd.print((int)oldThreshold);
        lcd.print(" C  ");
        lcd.setCursor(0, 3);
        lcd.print("New: ");
        lcd.print((int)tempThreshold);
        lcd.print(" C  ");

        digitalWrite(RED_LED_PIN,   i % 2);
        digitalWrite(GREEN_LED_PIN, i % 2);
        delay(500);
      }

      lcd.clear();
      digitalWrite(RED_LED_PIN,   LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }
}
