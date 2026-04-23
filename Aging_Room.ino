#include "config.h"
#include "auth.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include "storage.h"
#include <avr/wdt.h>

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

  // --- START 10-SECOND BOOT DISPLAY ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ethernet IP:");
  lcd.setCursor(0, 1);
  lcd.print(Ethernet.localIP());
  lcd.setCursor(0, 2);
  lcd.print("DNS Name:");

  const char* hostname = "agingroom00.mach.hq.seegrid.lan";
  char scrollText[48];
  snprintf(scrollText, sizeof(scrollText), "%s    ", hostname);
  int scrollLen = strlen(scrollText);
  unsigned long scrollStart = millis();
  int scrollPos = 0;
  
  while (millis() - scrollStart < 10000) {
    lcd.setCursor(0, 3);
    char displayLine[21];
    for (int i = 0; i < 20; i++) {
      displayLine[i] = scrollText[(scrollPos + i) % scrollLen];
    }
    displayLine[20] = '\0';
    lcd.print(displayLine);
    delay(300); 
    scrollPos = (scrollPos + 1) % scrollLen;
  }
  lcd.clear();
  // --- END BOOT DISPLAY ---

  Udp.begin(UDP_LOCAL_PORT);
  requestNtpTime();
  lastNtpCheck = false;

  initSDCard();
  lastDisplaySwitch = millis();
  server.begin();

  // Arm the Watchdog!
  wdt_enable(WDTO_8S); 
  Serial.println("Watchdog Timer Armed (8 Seconds)");
}

void loop() {
  wdt_reset(); // Pet the dog!

  unsigned long now = millis();
  static unsigned long lastEpochUpdate = millis();
  
  while (now - lastEpochUpdate >= 1000) {
    currentEpoch++;
    lastEpochUpdate += 1000;
  }

  // --- DEFERRED TIMESTAMPTED LOGGING ---
  static bool bootLogged = false;
  if (!bootLogged && currentEpoch > 1000000000UL) {
    File alertFile = SD.open("EVENTS.txt", FILE_WRITE);
    if (alertFile) {
      int y, mo, d, h, mi, s, wd;
      epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
      char timeBuf[64];
      snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d - Watchdog Recovery Restart", y, mo + 1, d, h, mi, s);
      alertFile.println(timeBuf);
      alertFile.close();
    }
    bootLogged = true;
  }

  handleButtonPress();
  readSensors();
  updateLEDs();
  updateDisplay();
  
  // --- NTP SYNC AT NOON AND MIDNIGHT ---
  // Syncs at 00:00 and 12:00 every calendar day regardless of boot time
  if (currentEpoch > 1000000000UL) {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    bool isNtpWindow = (h == 0 || h == 12) && (mi == 0);
    if (isNtpWindow && !lastNtpCheck) {
      wdt_disable();
      requestNtpTime();
      wdt_enable(WDTO_8S);
      lastNtpCheck = true;  // Prevent re-triggering within this minute
    }
    if (!isNtpWindow) {
      lastNtpCheck = false; // Reset when outside the sync window
    }
  }

  // --- The Midnight Janitor (6-Month Cleanup) ---
  static int lastPurgeDay = -1;
  if (currentEpoch > 1000000000UL) { 
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    
    if (h == 0 && mi == 0 && d != lastPurgeDay) {
      wdt_disable();
      purgeOldLogs();
      wdt_disable();
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
      wdt_disable();       // <-- Muzzle the dog! SD writes can be slow
      appendCsvData();
      wdt_enable(WDTO_8S); // <-- Re-arm the dog!
      
      lastCsvWrite = millis();
      nextLogEpoch += 300; 
    }
  } else {
    if (millis() - lastCsvWrite >= CSV_WRITE_INTERVAL) {
      wdt_disable();       // <-- Muzzle the dog!
      appendCsvData();
      wdt_enable(WDTO_8S); // <-- Re-arm the dog!
      
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

    // --- MEMORY OPTIMIZATION: fixed char[] instead of String objects ---
    // Eliminates heap allocation and fragmentation on every HTTP request
    char httpRequest[64];
    char currentLine[122];
    char authHeader[122];
    httpRequest[0]  = '\0';
    currentLine[0]  = '\0';
    authHeader[0]   = '\0';
    uint8_t httpRequestLen  = 0;
    uint8_t currentLineLen  = 0;

    while (client.connected()) {
      wdt_reset(); // Pet the dog while serving web requests

      if (client.available()) {
        char c = client.read();
        
        // 1. Capture the actual web request (e.g., GET /temp.csv)
        if (isFirstLine && httpRequestLen < 63 && c != '\r' && c != '\n') {
          httpRequest[httpRequestLen++] = c;
          httpRequest[httpRequestLen]   = '\0';
        }

        // 2. Capture headers line-by-line
        if (c != '\n' && c != '\r' && currentLineLen < 121) {
          currentLine[currentLineLen++] = c;
          currentLine[currentLineLen]   = '\0';
        }

        // 3. Blank line = end of headers — authenticate and route
        if (c == '\n' && currentLineIsBlank) {
          
          if (!checkAuth(authHeader)) {
            client.println("HTTP/1.1 401 Unauthorized");
            client.println("WWW-Authenticate: Basic realm=\"Seegrid Aging Room\"");
            client.println("Content-Type: text/html");
            client.println("Connection: close\n");
            client.println("<h2>401 Unauthorized</h2><p>Valid Seegrid credentials required.</p>");
            break;
          }
          
          if      (strncmp(httpRequest, "GET /threshold", 14) == 0) serveThreshold(client);
          else if (strncmp(httpRequest, "GET /status",    11) == 0) serveStatus(client);
          else if (strncmp(httpRequest, "GET /sysinfo",   12) == 0) serveSystemInfo(client);
          else if (strncmp(httpRequest, "GET /temp.csv",  13) == 0) serveFile(client, "temp.csv",  "text/csv");
          else if (strncmp(httpRequest, "GET /humid.csv", 14) == 0) serveFile(client, "humid.csv", "text/csv");

          // Watchdog Endpoints
          else if (strncmp(httpRequest, "GET /events",       11) == 0) serveFile(client, "EVENTS.txt", "text/plain");
          else if (strncmp(httpRequest, "GET /clear-events", 17) == 0) {
            SD.remove("EVENTS.txt");
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nAlerts Cleared!");
          }
          else if (strncmp(httpRequest, "GET /eject", 10) == 0) {
            client.println("HTTP/1.1 200 OK\nConnection: close\n");
            SD.end();
            lcd.clear(); lcd.print("SD UNMOUNTED"); lcd.setCursor(0,1); lcd.print("SAFE TO UNPLUG");
            while(1) { digitalWrite(RED_LED_PIN, HIGH); delay(200); digitalWrite(RED_LED_PIN, LOW); delay(200); wdt_reset(); }
          }
          
          // Archive Fetcher
          else if (strncmp(httpRequest, "GET /archive?file=", 18) == 0) {
            // Extract filename from fixed char buffer — no String needed
            char fileName[16];
            fileName[0] = '\0';
            uint8_t fnLen = 0;
            for (uint8_t i = 18; i < httpRequestLen && httpRequest[i] != ' ' && fnLen < 15; i++) {
              fileName[fnLen++] = httpRequest[i];
            }
            fileName[fnLen] = '\0';

            if (fnLen > 0 && SD.exists(fileName)) {
              File file = SD.open(fileName, FILE_READ);
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
          
          else if (strncmp(httpRequest, "GET /cleanup", 12) == 0) {
            SD.remove("temp.csv");
            SD.remove("humid.csv");
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nSUCCESS: Old temp.csv and humid.csv have been deleted!");
          }
          else if (strncmp(httpRequest, "GET / ", 6) == 0) serveRootPage(client);
          else {
            client.println("HTTP/1.1 404 Not Found\nConnection: close\n");
          }
          break;
        }
        
        // 4. Save Authorization header when we see it
        if (c == '\n') {
          if (strncmp(currentLine, "Authorization:", 14) == 0) {
            strncpy(authHeader, currentLine, 121);
            authHeader[121] = '\0';
          }
          currentLine[0]  = '\0';
          currentLineLen  = 0;
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
