#include "config.h"
#include "auth.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include "storage.h"
#include <avr/wdt.h>

void setup() {
  Serial.begin(115200);
  while (!Serial  && millis() < 3000);
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

  Udp.begin(UDP_LOCAL_PORT);
  requestNtpTime(); // Initial Boot Sync
  lastNtpCheck = millis();

  initSDCard();
  lastDisplaySwitch = millis();
  server.begin();
  wdt_enable(WDTO_8S); 
  Serial.println("Watchdog Timer Armed (8 Seconds)");
}

void loop() {
  wdt_reset(); 
  unsigned long now = millis();
  static unsigned long lastEpochUpdate = millis();
  while (now - lastEpochUpdate >= 1000) {
    currentEpoch++;
    lastEpochUpdate += 1000;
  }

  // --- Style 2 Universal Watchdog Logging ---
  static bool bootLogged = false;
  if (!bootLogged && currentEpoch > 1000000000UL) {
    File alertFile = SD.open("EVENTS.txt", FILE_WRITE);
    if (alertFile) {
      int y, mo, d, h, mi, s, wd;
      epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
      int dh = h % 12; if (dh == 0) dh = 12;
      const char* ampm = (h >= 12) ? "PM" : "AM";
      char timeBuf[80];
      snprintf(timeBuf, sizeof(timeBuf), "%02d/%02d/%04d %02d:%02d:%02d %s - Watchdog Recovery Restart", 
               mo + 1, d, y, dh, mi, s, ampm);
      alertFile.println(timeBuf);
      alertFile.close();
    }
    bootLogged = true;
  }

  handleButtonPress();
  readSensors();
  updateLEDs();
  updateDisplay();

  // --- NTP Sync: Triggered at 12:00 AM and 12:00 PM Calendar Time ---
  if (currentEpoch > 1000000000UL) {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    static int lastSyncHour = -1;
    if ((h == 0 || h == 12) && mi == 0 && h != lastSyncHour) {
      wdt_disable();       
      requestNtpTime();
      wdt_enable(WDTO_8S); 
      lastSyncHour = h;
      lastNtpCheck = millis();
      Serial.print("Scheduled NTP Sync Performed at: "); Serial.println(h);
    }
  }

  // --- Midnight Janitor ---
  static int lastPurgeDay = -1;
  if (currentEpoch > 1000000000UL) { 
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    if (h == 0 && mi == 0 && d != lastPurgeDay) {
      purgeOldLogs();
      lastPurgeDay = d; 
    }
  }

  // --- Time-Aligned Logging (CSV Write) ---
  static unsigned long nextLogEpoch = 0;
  if (currentEpoch > 1000000000UL) { 
    if (nextLogEpoch == 0) {
      nextLogEpoch = currentEpoch + (300 - (currentEpoch % 300));
    }
    if (currentEpoch >= nextLogEpoch) {
      wdt_disable();       
      appendCsvData();
      wdt_enable(WDTO_8S); 
      lastCsvWrite = millis();
      nextLogEpoch += 300; 
    }
  } else {
    if (millis() - lastCsvWrite >= CSV_WRITE_INTERVAL) {
      wdt_disable();
      appendCsvData();
      wdt_enable(WDTO_8S); 
      lastCsvWrite = millis();
    }
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
      client.stop();
      return;
    }
    bool currentLineIsBlank = true;
    bool isFirstLine = true; 
    String httpRequest = "";
    String currentLine = "";  
    String authHeader = "";
    httpRequest.reserve(64); 
    currentLine.reserve(120);
    while (client.connected()) {
      wdt_reset(); 
      if (client.available()) {
        char c = client.read();
        if (isFirstLine && httpRequest.length() < 60 && c != '\r' && c != '\n') {
          httpRequest += c;
        }
        if (c != '\n' && c != '\r' && currentLine.length() < 120) {
          currentLine += c;
        }
        if (c == '\n' && currentLineIsBlank) {
          if (!checkAuth(authHeader + "\n")) {
            client.println("HTTP/1.1 401 Unauthorized");
            client.println("WWW-Authenticate: Basic realm=\"Seegrid Aging Room\"");
            client.println("Content-Type: text/html");
            client.println("Connection: close\n");
            client.println("<h2>401 Unauthorized</h2><p>Valid Seegrid credentials required.</p>");
            break; 
          }
          if (httpRequest.startsWith("GET /threshold")) serveThreshold(client);
          else if (httpRequest.startsWith("GET /status")) serveStatus(client);
          else if (httpRequest.startsWith("GET /sysinfo")) serveSystemInfo(client);
          else if (httpRequest.startsWith("GET /temp.csv")) serveFile(client, "temp.csv", "text/csv");
          else if (httpRequest.startsWith("GET /humid.csv")) serveFile(client, "humid.csv", "text/csv");
          else if (httpRequest.startsWith("GET /events")) serveFile(client, "EVENTS.txt", "text/plain");
          else if (httpRequest.startsWith("GET /clear-events")) {
            SD.remove("EVENTS.txt");
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nAlerts Cleared!");
          }
          else if (httpRequest.startsWith("GET /eject")) {
            client.println("HTTP/1.1 200 OK\nConnection: close\n");
            SD.end();
            lcd.clear(); lcd.print("SD UNMOUNTED"); lcd.setCursor(0,1); lcd.print("SAFE TO UNPLUG");
            while(1) { digitalWrite(RED_LED_PIN, HIGH); delay(200); digitalWrite(RED_LED_PIN, LOW); delay(200); wdt_reset(); }
          }
          else if (httpRequest.startsWith("GET /archive?file=")) {
            int startIdx = 18;
            int endIdx = httpRequest.indexOf(' ', startIdx);
            if (endIdx != -1) {
              String fileName = httpRequest.substring(startIdx, endIdx);
              if (SD.exists(fileName.c_str())) {
                File file = SD.open(fileName.c_str(), FILE_READ);
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/csv");
                client.print("Content-Disposition: attachment; filename=\"");
                client.print(fileName);
                client.println("\"");
                client.println("Connection: close\n");
                while (file.available()) { 
                  client.write(file.read());
                  wdt_reset(); 
                }
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
          if (currentLine.startsWith("Authorization:")) {
            authHeader = currentLine;
          }
          currentLine = "";
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