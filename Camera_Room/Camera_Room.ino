// ============================================================
// Camera Room — Arduino UNO RS485 Responder
// Waits for Mega poll request, reads DHT22, sends reply
// Protocol: Mega sends "GET:CAM\n", UNO replies "CAM:21.5,45.2\n"
// ============================================================

#include <DHT.h>
#include <SoftwareSerial.h>

// --- Pin Definitions ---
#define DHT_PIN       4     // DHT22 data pin
#define RS485_TX_PIN  11    // MAX485 DI
#define RS485_RX_PIN  8     // MAX485 RO
#define RS485_DE_PIN  10    // MAX485 DE+RE — HIGH to transmit, LOW to receive
#define DHTTYPE       DHT22

// --- Timing ---
// How long to wait for a complete poll request before discarding partial data
#define REQUEST_TIMEOUT 500UL

// --- Objects ---
DHT dht(DHT_PIN, DHTTYPE);
SoftwareSerial rs485(RS485_RX_PIN, RS485_TX_PIN);

// --- Receive buffer ---
char rxBuf[16];
uint8_t rxLen = 0;
unsigned long lastByteTime = 0;

void setup() {
  digitalWrite(RS485_DE_PIN, LOW);  // Receive mode — do this BEFORE pinMode
  pinMode(RS485_DE_PIN, OUTPUT);
  Serial.begin(115200);
  rs485.begin(9600);
  dht.begin();
  delay(2000);  // DHT22 stabilise
  Serial.println(F("Camera Room UNO Ready — poll mode"));
}

void loop() {
  // Discard stale partial receive buffer after timeout
  if (rxLen > 0 && millis() - lastByteTime > REQUEST_TIMEOUT) {
    rxLen = 0;
  }

  while (rs485.available()) {
    char c = rs485.read();
    lastByteTime = millis();

    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (strcmp(rxBuf, "GET:CAM") == 0) {
        respondToMega();
      }
      rxLen = 0;
    } else if (c != '\r' && rxLen < sizeof(rxBuf) - 1) {
      rxBuf[rxLen++] = c;
    }
  }
}

void respondToMega() {
  float temp  = dht.readTemperature();
  float humid = dht.readHumidity();

  // Retry once on failed read — wait full 2s and re-read both
  if (isnan(temp) || isnan(humid) || temp <= -40.0) {
    delay(2000);
    temp  = dht.readTemperature();
    humid = dht.readHumidity();
  }

  char packet[32];

  if (isnan(temp) || isnan(humid) || temp <= -40.0 || humid < 0.0) {
    snprintf(packet, sizeof(packet), "CAM:ERR,ERR\n");
  } else {
    char tempStr[8];
    char humidStr[8];
    dtostrf(temp,  4, 1, tempStr);
    dtostrf(humid, 4, 1, humidStr);
    snprintf(packet, sizeof(packet), "CAM:%s,%s\n", tempStr, humidStr);
  }

  // Transmit reply
  digitalWrite(RS485_DE_PIN, HIGH);
  delay(1);
  rs485.print(packet);
  rs485.flush();
  delay(1);
  digitalWrite(RS485_DE_PIN, LOW);  // Release bus immediately

  Serial.print(F("Replied: "));
  Serial.print(packet);
}
