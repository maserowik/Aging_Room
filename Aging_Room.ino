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
  readRS485();
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
      wdt_enable(WDTO_8S);
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

          // --- Skit Room endpoints ---
          else if (strncmp(httpRequest, "GET /skit/status",         16) == 0) serveSkitStatus(client);
          else if (strncmp(httpRequest, "GET /skit/sysinfo",        17) == 0) serveSkitSysInfo(client);
          else if (strncmp(httpRequest, "GET /skit/threshold/temp", 24) == 0) serveSkitThresholdTemp(client);
          else if (strncmp(httpRequest, "GET /skit/threshold/humid",25) == 0) serveSkitThresholdHumid(client);
          else if (strncmp(httpRequest, "POST /skit/threshold/temp", 25) == 0) {
            // Value passed as query string: POST /skit/threshold/temp?v=22.5
            const char* q = strchr(httpRequest + 25, '=');
            if (q) { extern float skitTempThreshold; skitTempThreshold = atof(q + 1); EEPROM.put(EEPROM_SKIT_TEMP_THRESHOLD_ADDR, skitTempThreshold); }
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nOK");
          }
          else if (strncmp(httpRequest, "POST /skit/threshold/humid", 26) == 0) {
            const char* q = strchr(httpRequest + 26, '=');
            if (q) { extern float skitHumidThreshold; skitHumidThreshold = atof(q + 1); EEPROM.put(EEPROM_SKIT_HUMID_THRESHOLD_ADDR, skitHumidThreshold); }
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nOK");
          }
          else if (strncmp(httpRequest, "GET /skit/temp.csv",  18) == 0) serveFile(client, "SK_T.csv",  "text/csv");
          else if (strncmp(httpRequest, "GET /skit/humid.csv", 19) == 0) serveFile(client, "SK_H.csv",  "text/csv");
          else if (strncmp(httpRequest, "GET /skit ",          10) == 0) serveSkitPage(client);

          // --- Camera Room endpoints ---
          else if (strncmp(httpRequest, "GET /camera/status",         18) == 0) serveCameraStatus(client);
          else if (strncmp(httpRequest, "GET /camera/sysinfo",        19) == 0) serveCameraSysInfo(client);
          else if (strncmp(httpRequest, "GET /camera/threshold/temp", 26) == 0) serveCameraThresholdTemp(client);
          else if (strncmp(httpRequest, "GET /camera/threshold/humid",27) == 0) serveCameraThresholdHumid(client);
          else if (strncmp(httpRequest, "POST /camera/threshold/temp", 27) == 0) {
            const char* q = strchr(httpRequest + 27, '=');
            if (q) { extern float camTempThreshold; camTempThreshold = atof(q + 1); EEPROM.put(EEPROM_CAM_TEMP_THRESHOLD_ADDR, camTempThreshold); }
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nOK");
          }
          else if (strncmp(httpRequest, "POST /camera/threshold/humid", 28) == 0) {
            const char* q = strchr(httpRequest + 28, '=');
            if (q) { extern float camHumidThreshold; camHumidThreshold = atof(q + 1); EEPROM.put(EEPROM_CAM_HUMID_THRESHOLD_ADDR, camHumidThreshold); }
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nOK");
          }
          else if (strncmp(httpRequest, "GET /camera/temp.csv",  20) == 0) serveFile(client, "CA_T.csv",  "text/csv");
          else if (strncmp(httpRequest, "GET /camera/humid.csv", 21) == 0) serveFile(client, "CA_H.csv",  "text/csv");
          else if (strncmp(httpRequest, "GET /camera ",         12) == 0) serveCameraPage(client);

          // --- Admin endpoints (hidden — no nav link) ---
          else if (strncmp(httpRequest, "GET /admin/delete-all", 21) == 0) {
            handleAdminDeleteAll(client);
          }
          else if (strncmp(httpRequest, "GET /admin/delete?file=", 23) == 0) {
            // Extract filename from query string
            char delFile[16];
            delFile[0] = '\0';
            uint8_t dfLen = 0;
            for (uint8_t i = 23; i < httpRequestLen && httpRequest[i] != ' ' && dfLen < 15; i++) {
              delFile[dfLen++] = httpRequest[i];
            }
            delFile[dfLen] = '\0';
            handleAdminDelete(client, delFile);
          }
          else if (strncmp(httpRequest, "GET /admin", 10) == 0) serveAdminPage(client);

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
