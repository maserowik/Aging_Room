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

// --- RS485 poll timing ---
// Mega polls Skit then Camera sequentially
// Clock-aligned to 5-minute boundaries — matches CSV write schedule
#define RS485_DE_PIN        34         // Mega MAX485 DE+RE — pin 34
#define RS485_REPLY_TIMEOUT 2000UL     // Wait up to 2 seconds for a reply

// --- Stale sensor flags --- ensures stale warning prints once only ---
static bool skitStaleWarned = false;
static bool camStaleWarned  = false;

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

  // RS485 DE pin — start in receive mode
  digitalWrite(RS485_DE_PIN, LOW);
  pinMode(RS485_DE_PIN, OUTPUT);

  // Start RS485 hardware serial
  Serial1.begin(RS485_BAUD);

  Serial.print(F("Aging Room threshold: "));   Serial.println(tempThreshold);
  Serial.print(F("Skit temp threshold: "));    Serial.println(skitTempThreshold);
  Serial.print(F("Skit humid threshold: "));   Serial.println(skitHumidThreshold);
  Serial.print(F("Camera temp threshold: "));  Serial.println(camTempThreshold);
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

  // Mark Skit sensor stale if no reply within timeout
  if (lastSkitReceive > 0 && (now - lastSkitReceive > RS485_TIMEOUT_MS)) {
    if (!skitStaleWarned) {
      Serial.println(F("Skit stale"));
      skitStaleWarned = true;
    }
    tSkit = NAN;
    hSkit = NAN;
  }

  // Mark Camera sensor stale if no reply within timeout
  if (lastCamReceive > 0 && (now - lastCamReceive > RS485_TIMEOUT_MS)) {
    if (!camStaleWarned) {
      Serial.println(F("Cam stale"));
      camStaleWarned = true;
    }
    tCam = NAN;
    hCam = NAN;
  }
}

// ============================================================
// parseReply() — internal helper
// Parses a null-terminated reply line from a UNO
// Expected formats: SKIT:21.5,45.2  or  CAM:21.5,45.2  or  SKIT:ERR,ERR
// Updates the appropriate globals directly
// ============================================================
static void parseReply(char* buf) {
  bool isSkit = (strncmp(buf, "SKIT:", 5) == 0);
  bool isCam  = (strncmp(buf, "CAM:",  4) == 0);

  if (!isSkit && !isCam) {
    Serial.print(F("RS485 unknown reply: "));
    Serial.println(buf);
    return;
  }

  const char* data = isSkit ? buf + 5 : buf + 4;
  const char* comma = strchr(data, ',');
  if (!comma) return;

  char tempBuf[8];
  char humidBuf[8];
  uint8_t tLen = comma - data;
  if (tLen >= sizeof(tempBuf)) return;

  memcpy(tempBuf, data, tLen);
  tempBuf[tLen] = '\0';
  strncpy(humidBuf, comma + 1, sizeof(humidBuf) - 1);
  humidBuf[sizeof(humidBuf) - 1] = '\0';

  bool isErr = (strcmp(tempBuf, "ERR") == 0);

  if (!isErr) {
    float parsedTemp  = atof(tempBuf);
    float parsedHumid = atof(humidBuf);

    // Sanity check — reject physically impossible values
    if (parsedTemp < 5.0 || parsedTemp > 50.0 ||
        parsedHumid < 1.0 || parsedHumid > 99.0) {
      Serial.print(F("RS485 bad value: "));
      Serial.println(buf);
      return;
    }

    if (isSkit) {
      tSkit = parsedTemp;
      hSkit = parsedHumid;
      lastSkitReceive = millis();
      skitStaleWarned = false;
      Serial.print(F("Skit: "));
      Serial.print(tSkit); Serial.print(F("C, "));
      Serial.print(hSkit); Serial.println(F("%"));
    } else {
      tCam = parsedTemp;
      hCam = parsedHumid;
      lastCamReceive = millis();
      camStaleWarned = false;
      Serial.print(F("Camera: "));
      Serial.print(tCam); Serial.print(F("C, "));
      Serial.print(hCam); Serial.println(F("%"));
    }
  } else {
    // ERR reply — mark NAN but update timestamp so stale watchdog resets
    if (isSkit) {
      tSkit = NAN; hSkit = NAN;
      lastSkitReceive = millis();
      skitStaleWarned = false;
      Serial.println(F("Skit: ERR"));
    } else {
      tCam = NAN; hCam = NAN;
      lastCamReceive = millis();
      camStaleWarned = false;
      Serial.println(F("Camera: ERR"));
    }
  }
}

// ============================================================
// pollNode() — internal helper
// Sends a poll request to one node and waits for its reply
// request: "GET:SKIT\n" or "GET:CAM\n"
// Returns true if a reply was received and parsed
// ============================================================
static bool pollNode(const char* request) {
  // Flush any stale bytes in the receive buffer first
  while (Serial1.available()) Serial1.read();

  // Transmit the poll request
  digitalWrite(RS485_DE_PIN, HIGH);
  delay(1);
  Serial1.print(request);
  Serial1.flush();
  delay(1);
  digitalWrite(RS485_DE_PIN, LOW);

  // Wait for reply with timeout
  char replyBuf[32];
  uint8_t replyLen = 0;
  unsigned long deadline = millis() + RS485_REPLY_TIMEOUT;

  while (millis() < deadline) {
    if (Serial1.available()) {
      char c = Serial1.read();
      if (c == '\n') {
        replyBuf[replyLen] = '\0';
        if (replyLen > 0) {
          parseReply(replyBuf);
          return true;
        }
      } else if (c != '\r' && replyLen < sizeof(replyBuf) - 1) {
        replyBuf[replyLen++] = c;
      }
    }
    wdt_reset();  // Keep watchdog happy during wait
  }

  // Timeout — no reply received
  Serial.print(F("RS485 no reply: "));
  Serial.print(request);
  return false;
}

// ============================================================
// readRS485() — called every loop() iteration from Aging_Room.ino
// On boot: polls immediately for fresh data, then aligns to
// 5-minute clock boundaries (:00, :05, :10 ... :55)
// Only one node transmits at a time — bus contention impossible
// ============================================================
void readRS485() {
  extern unsigned long currentEpoch;
  static unsigned long nextPollEpoch = 0;
  static bool bootPollDone = false;

  // Wait until NTP time is valid
  if (currentEpoch < 1000000000UL) return;

  // On first valid NTP time — do immediate boot poll then set schedule
  if (!bootPollDone) {
    bootPollDone = true;
    nextPollEpoch = currentEpoch + (300 - (currentEpoch % 300));
    wdt_disable();
    pollNode("GET:SKIT\n");
    delay(50);
    pollNode("GET:CAM\n");
    wdt_enable(WDTO_8S);
    return;
  }

  // Not yet time for next scheduled poll
  if (currentEpoch < nextPollEpoch) return;

  // On schedule — advance to next boundary and poll
  nextPollEpoch += 300;

  wdt_disable();             // Disable watchdog during poll — up to 4 seconds total
  pollNode("GET:SKIT\n");
  delay(50);                 // Brief gap between polls
  pollNode("GET:CAM\n");
  wdt_enable(WDTO_8S);       // Re-arm watchdog
}

// ============================================================
void updateLEDs() {
  unsigned long now = millis();

  bool anyError =
    isnan(tA) || isnan(tB) || isnan(tC) || isnan(tD) ||
    isnan(tSkit) || isnan(hSkit) ||
    isnan(tCam)  || isnan(hCam);

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
  if (anyError)           blinkInterval = BLINK_INTERVAL_FAST;
  else if (anyOutOfRange) blinkInterval = BLINK_INTERVAL_NORMAL;
  else                    blinkInterval = 0;

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