// ============================================================
// Camera Room — Arduino UNO RS485 Transmitter
// Reads one DHT22 sensor and transmits temp/humidity to Mega
// Transmit interval: every 7 minutes
// Packet format: CAM:21.5,45.2\n
// ============================================================

#include <DHT.h>
#include <SoftwareSerial.h>

// --- Pin Definitions ---
#define DHT_PIN       4     // DHT22 data pin
#define RS485_TX_PIN  11    // MAX485 DI
#define RS485_RX_PIN  8     // MAX485 RO (not used — receive only on Mega)
#define RS485_DE_PIN  10    // MAX485 DE+RE — HIGH to transmit, LOW to receive
#define DHTTYPE       DHT22

// --- Timing ---
#define TRANSMIT_INTERVAL 420000UL  // 7 minutes in ms

// --- Objects ---
DHT dht(DHT_PIN, DHTTYPE);
SoftwareSerial rs485(RS485_RX_PIN, RS485_TX_PIN);

// --- Globals ---
unsigned long lastTransmit = 0;

void setup() {
  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);  // Bus released — receive mode by default
  Serial.begin(115200);             // Debug only
  rs485.begin(9600);
  dht.begin();
  delay(2000);                      // Allow DHT22 to stabilize on power-up
  Serial.println("Camera Room UNO Ready");
}

void loop() {
  unsigned long now = millis();

  if (now - lastTransmit >= TRANSMIT_INTERVAL) {
    lastTransmit = now;
    transmitData();
  }
}

void transmitData() {
  float temp = dht.readTemperature();
  float humid = dht.readHumidity();

  // Retry once on failed read
  if (isnan(temp))  { delay(100); temp  = dht.readTemperature(); }
  if (isnan(humid)) { delay(100); humid = dht.readHumidity(); }

  char packet[32];

  if (isnan(temp) || isnan(humid)) {
    // Send ERR packet so Mega knows sensor failed
    snprintf(packet, sizeof(packet), "CAM:ERR,ERR\n");
  } else {
    // Format: CAM:21.5,45.2\n
    char tempStr[8];
    char humidStr[8];
    dtostrf(temp,  4, 1, tempStr);
    dtostrf(humid, 4, 1, humidStr);
    snprintf(packet, sizeof(packet), "CAM:%s,%s\n", tempStr, humidStr);
  }

  // Enable driver — take control of the bus
  digitalWrite(RS485_DE_PIN, HIGH);
  delay(1);                         // Brief settling time before transmit
  rs485.print(packet);
  rs485.flush();                    // Wait for transmission to complete
  delay(1);                         // Brief settling time after transmit
  digitalWrite(RS485_DE_PIN, LOW);  // Release the bus

  // Debug to Serial Monitor
  Serial.print("Sent: ");
  Serial.print(packet);
}
