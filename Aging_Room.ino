#include "config.h"
#include "auth.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include "storage.h"
#include <avr/wdt.h>

void setup() {
  Serial.begin(115200);
  // Timeout after 3 seconds so the system boots fully without a USB monitor connected
  while (!Serial && millis() < 3000);
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
  requestNtpTime();  // Initial boot sync
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

  // --- Watchdog Recovery Boot Log ---
  // Writes one entry to EVENTS.txt on every boot.
  // If NTP has already synced (epoch valid), use the real timestamp.
  // If NTP has not yet synced, log with a placeholder so no event is silently lost.
  static bool bootLogged = false;
  if (!bootLogged) {
    File alertFile = SD.open("EVENTS.txt", FILE_WRITE);
    if (alertFile) {
      char timeBuf[80];
      if (currentEpoch >= EPOCH_VALID_THRESHOLD) {
        int y, mo, d, h, mi, s, wd;
        epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
        int dh = h % 12; if (dh == 0) dh = 12;
        const char* ampm = (h >= 12) ? "PM" : "AM";
        snprintf(timeBuf, sizeof(timeBuf),
                 "%02d/%02d/%04d %02d:%02d:%02d %s - Watchdog Recovery Restart",
                 mo + 1, d, y, dh, mi, s, ampm);
      } else {
        // NTP not yet valid — log with explicit notice so the event is not silently dropped
        snprintf(timeBuf, sizeof(timeBuf),
                 "(NTP not synced) - Watchdog Recovery Restart");
      }
      alertFile.println(timeBuf);
      alertFile.close();
    }
    bootLogged = true;
  }

  handleButtonPress();
  readSensors();
  updateLEDs();
  updateDisplay();

  // --- NTP Sync: Triggered at 12:00 AM and 12:00 PM calendar time ---
  if (currentEpoch >= EPOCH_VALID_THRESHOLD) {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    static int lastSyncHour = -1;
    if ((h == 0 || h == 12) && mi == 0 && h != lastSyncHour) {
      // Disarm watchdog during NTP: tryNtpSync() can block up to 3 seconds per server
      // (two servers = up to 6 seconds total), which is dangerously close to the 8-second
      // watchdog window. Disarming guarantees no false reboot during normal NTP operation.
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
  if (currentEpoch >= EPOCH_VALID_THRESHOLD) {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    if (h == 0 && mi == 0 && d != lastPurgeDay) {
      purgeOldLogs();
      lastPurgeDay = d;
    }
  }

  // --- Time-Aligned Logging (CSV Write every 5 minutes on clock boundaries) ---
  static unsigned long nextLogEpoch = 0;
  if (currentEpoch >= EPOCH_VALID_THRESHOLD) {
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
    // NTP not yet valid — fall back to millis()-based interval so data is not lost
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

    // FIXED: static char arrays instead of heap-allocated String objects,
    // eliminating fragmentation on every request.
    static char httpRequest[64];
    static char currentLine[128];
    static char authHeader[128];
    httpRequest[0]  = '\0';
    currentLine[0]  = '\0';
    authHeader[0]   = '\0';
    int httpReqLen  = 0;
    int curLineLen  = 0;
    int authHdrLen  = 0;

    while (client.connected()) {
      wdt_reset();
      if (client.available()) {
        char c = client.read();

        // Accumulate first line (the request line: "GET /path HTTP/1.1")
        if (isFirstLine && httpReqLen < (int)sizeof(httpRequest) - 1 && c != '\r' && c != '\n') {
          httpRequest[httpReqLen++] = c;
          httpRequest[httpReqLen]   = '\0';
        }

        // Accumulate the current header line
        if (c != '\n' && c != '\r' && curLineLen < (int)sizeof(currentLine) - 1) {
          currentLine[curLineLen++] = c;
          currentLine[curLineLen]   = '\0';
        }

        if (c == '\n' && currentLineIsBlank) {
          // End of HTTP headers — authenticate then route
          // Build a temporary String just for checkAuth (it does its own parsing internally)
          String authStr = String(authHeader) + "\n";
          if (!checkAuth(authStr)) {
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
          else if (strncmp(httpRequest, "GET /events",    11) == 0) serveFile(client, "EVENTS.txt", "text/plain");
          else if (strncmp(httpRequest, "GET /clear-events", 17) == 0) {
            SD.remove("EVENTS.txt");
            client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\nAlerts Cleared!");
          }
          else if (strncmp(httpRequest, "GET /eject", 10) == 0) {
            client.println("HTTP/1.1 200 OK\nConnection: close\n");
            SD.end();
            lcd.clear(); lcd.print("SD UNMOUNTED"); lcd.setCursor(0,1); lcd.print("SAFE TO UNPLUG");
            while (1) { digitalWrite(RED_LED_PIN, HIGH); delay(200); digitalWrite(RED_LED_PIN, LOW); delay(200); wdt_reset(); }
          }
          else if (strncmp(httpRequest, "GET /archive?file=", 18) == 0) {
            // Extract filename from the request line
            char fileName[20];
            int startIdx = 18;
            int endIdx = startIdx;
            while (httpRequest[endIdx] != ' ' && httpRequest[endIdx] != '\0') endIdx++;
            int fnLen = endIdx - startIdx;
            if (fnLen > 0 && fnLen < (int)sizeof(fileName)) {
              memcpy(fileName, httpRequest + startIdx, fnLen);
              fileName[fnLen] = '\0';
              if (SD.exists(fileName)) {
                File file = SD.open(fileName, FILE_READ);
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/csv");
                client.print("Content-Disposition: attachment; filename=\"");
                client.print(fileName);
                client.println("\"");
                client.println("Connection: close\n");
                byte buf[128];
                while (file.available()) {
                  int n = file.read(buf, sizeof(buf));
                  client.write(buf, n);
                  wdt_reset();
                }
                file.close();
              } else {
                client.println("HTTP/1.1 404 Not Found\nConnection: close\n");
              }
            } else {
              client.println("HTTP/1.1 400 Bad Request\nConnection: close\n");
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

        if (c == '\n') {
          // Check if this completed line is the Authorization header
          if (strncmp(currentLine, "Authorization:", 14) == 0) {
            memcpy(authHeader, currentLine, curLineLen + 1);
            authHdrLen = curLineLen;
          }
          curLineLen = 0;
          currentLine[0] = '\0';
          currentLineIsBlank = true;
          isFirstLine = false;
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
