#include "config.h"
#include "auth.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include "storage.h"

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  initDisplay();
  initConnectionTracking();
  
  bootSequence();

  initSensors();

  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  Ethernet.init(10);

  Serial.println("Starting Ethernet with DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP failed. Trying static IP...");

     IPAddress ip(192, 168, 48, 20);
     IPAddress gateway(192, 168, 48, 1);
     IPAddress subnet(255, 255, 255, 0);
    
    Ethernet.begin(mac, ip, gateway, subnet);
    Serial.println("Static IP assigned.");
  } else {
    Serial.println("DHCP successful.");
  }

  delay(1000);
  Serial.print("Ethernet IP: ");
  Serial.println(Ethernet.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ethernet IP: ");
  lcd.setCursor(0, 2);
  lcd.print(Ethernet.localIP());
  delay(10000);
  lcd.clear();

  Udp.begin(UDP_LOCAL_PORT);
  requestNtpTime();
  lastNtpCheck = millis();

  initSDCard();

  lastDisplaySwitch = millis();
  server.begin();
}

void loop() {
  unsigned long now = millis();
  
  static unsigned long lastEpochUpdate = 0;
  if (now - lastEpochUpdate >= 1000) {
    currentEpoch++;
    lastEpochUpdate = now;
  }

  handleButtonPress();
  readSensors();
  updateLEDs();
  updateDisplay();

  if (millis() - lastNtpCheck >= NTP_INTERVAL) {
    requestNtpTime();
    lastNtpCheck = millis();
  }

  if (millis() - lastCsvWrite >= CSV_WRITE_INTERVAL) {
    appendCsvData();
    lastCsvWrite = millis();
  }

  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 30000) {
    cleanupStaleConnections();
    lastCleanup = millis();
  }

  EthernetClient client = server.available();
  if (client) {
    IPAddress clientIP = client.remoteIP();
    
    if (!canAcceptConnection(clientIP)) {
      sendServiceUnavailable(client);
      delay(1);
      client.stop();
      return;
    }

    bool currentLineIsBlank = true;
    String httpRequest = "";
    unsigned long connectionStart = millis();
    const unsigned long requestTimeout = 5000;

    while (client.connected()) {
      if (millis() - connectionStart > requestTimeout) {
        Serial.println("Request timeout");
        break;
      }

      if (client.available()) {
        char c = client.read();
        httpRequest += c;

        if (httpRequest.length() > 512) {
          client.println("HTTP/1.1 413 Request Entity Too Large");
          client.println("Connection: close");
          client.println();
          break;
        }

        if (c == '\n' && currentLineIsBlank) {
          if (httpRequest.startsWith("GET /temp.csv")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              serveFile(client, "temp.csv", "text/csv");
            }
            break;
          } else if (httpRequest.startsWith("GET /humid.csv")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              serveFile(client, "humid.csv", "text/csv");
            }
            break;
          } else if (httpRequest.startsWith("GET /delete_temp")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              SD.remove("temp.csv");
              createCsvHeaderIfNeeded();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Temperature CSV deleted.");
            }
            break;
          } else if (httpRequest.startsWith("GET /delete_humid")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              SD.remove("humid.csv");
              createCsvHeaderIfNeeded();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Humidity CSV deleted.");
            }
            break;
          } else if (httpRequest.startsWith("GET / ") || httpRequest.startsWith("GET / HTTP")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              serveRootPage(client);
            }
            break;
          } else {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println();
            client.println("404 Not Found");
            break;
          }
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    
    delay(1);
    client.stop();
    releaseConnection(clientIP);
  }
}