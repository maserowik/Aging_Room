#include "config.h"
#include "auth.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include "storage.h"

void setup() {
  Serial.begin(115200);
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
  }

  // --- START 5-SECOND BOOT DISPLAY ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ethernet IP:");
  lcd.setCursor(0, 1);
  lcd.print(Ethernet.localIP());
  lcd.setCursor(0, 2);
  lcd.print("DNS Name:");

  const char* hostname = "agingroom00.mach.hq.seegrid.lan";
  String scrollText = String(hostname) + "    ";
  int scrollLen = scrollText.length();
  unsigned long scrollStart = millis();
  int scrollPos = 0;
  
  while (millis() - scrollStart < 10000) {
    lcd.setCursor(0, 3);
    String display = "";
    for (int i = 0; i < 20; i++) {
      display += scrollText[(scrollPos + i) % scrollLen];
    }
    lcd.print(display);
    delay(300); 
    scrollPos = (scrollPos + 1) % scrollLen;
  }
  lcd.clear();
  // --- END BOOT DISPLAY ---

  Udp.begin(UDP_LOCAL_PORT);
  requestNtpTime();
  lastNtpCheck = millis();

  initSDCard();
  lastDisplaySwitch = millis();
  server.begin();
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastEpochUpdate = millis();
  
  while (now - lastEpochUpdate >= 1000) {
    currentEpoch++;
    lastEpochUpdate += 1000;
  }

  handleButtonPress();
  readSensors();
  updateLEDs();
  updateDisplay();
  
  if (millis() - lastNtpCheck >= NTP_INTERVAL) {
    requestNtpTime();
    lastNtpCheck = millis();
  }

  // --- The Midnight Janitor (6-Month Cleanup) ---
  static int lastPurgeDay = -1;
  if (currentEpoch > 1000000000UL) { 
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    
    // If exactly midnight and not yet purged today
    if (h == 0 && mi == 0 && d != lastPurgeDay) {
      purgeOldLogs();
      lastPurgeDay = d; 
    }
  }

  // --- Time-Aligned Logging ---
  static unsigned long nextLogEpoch = 0;
  if (currentEpoch > 1000000000UL) { 
    if (nextLogEpoch == 0) {
      nextLogEpoch = currentEpoch + (300 - (currentEpoch % 300));
    }
    if (currentEpoch >= nextLogEpoch) {
      appendCsvData();
      lastCsvWrite = millis();
      nextLogEpoch += 300; 
    }
  } else {
    if (millis() - lastCsvWrite >= CSV_WRITE_INTERVAL) {
      appendCsvData();
      lastCsvWrite = millis();
    }
  }

  // Socket Cleanup
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
      client.stop();
      return;
    }

    bool currentLineIsBlank = true;
    bool isFirstLine = true; 
    String httpRequest = "";
    httpRequest.reserve(64); 

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (isFirstLine && httpRequest.length() < 60 && c != '\r' && c != '\n') {
          httpRequest += c;
        }

        if (c == '\n' && currentLineIsBlank) {
          
          if (httpRequest.startsWith("GET /threshold")) serveThreshold(client);
          else if (httpRequest.startsWith("GET /status")) serveStatus(client);
          else if (httpRequest.startsWith("GET /sysinfo")) serveSystemInfo(client);
          else if (httpRequest.startsWith("GET /temp.csv")) serveFile(client, "temp.csv", "text/csv");
          else if (httpRequest.startsWith("GET /humid.csv")) serveFile(client, "humid.csv", "text/csv");
          
          // --- The Archive File Fetcher ---
          else if (httpRequest.startsWith("GET /archive?file=")) {
            int startIdx = 18;
            int endIdx = httpRequest.indexOf(' ', startIdx);
            if (endIdx != -1) {
              String fileName = httpRequest.substring(startIdx, endIdx);
              // Serve specifically as a direct download attachment
              if (SD.exists(fileName.c_str())) {
                File file = SD.open(fileName.c_str(), FILE_READ);
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/csv");
                client.print("Content-Disposition: attachment; filename=\"");
                client.print(fileName);
                client.println("\"");
                client.println("Connection: close\n");
                while (file.available()) { client.write(file.read()); }
                file.close();
              } else {
                client.println("HTTP/1.1 404 Not Found\nConnection: close\n");
              }
            }
          }
          
          else if (httpRequest.startsWith("GET /cleanup")) {
            SD.remove("temp.csv");
            SD.remove("humid.csv");
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nSUCCESS: Old temp.csv and humid.csv have been deleted!");
          }
          else if (httpRequest.startsWith("GET / ")) serveRootPage(client);
          else {
            client.println("HTTP/1.1 404 Not Found\nConnection: close\n");
          }
          break;
        }
        
        if (c == '\n') {
          currentLineIsBlank = true;
          isFirstLine = false; 
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    client.stop();
    releaseConnection(clientIP);
  }
}