#include "storage.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include <Ethernet.h>
#include <SD.h>
#include <avr/wdt.h>

extern LiquidCrystal_I2C lcd;

// Cached SD status — set once in initSDCard(), avoids calling SD.begin() repeatedly
static bool sdReady = false;

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

unsigned long lastCsvWrite = 0;

void initSDCard() {
  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("SD card initialization failed!");
    sdReady = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD Init Failed!");
    while (true) {
      digitalWrite(RED_LED_PIN, HIGH);
      delay(500);
      digitalWrite(RED_LED_PIN, LOW);
      delay(500);
      wdt_reset(); 
    }
  } else {
    Serial.println("SD card initialized.");
    sdReady = true;
  }
}

void createCsvHeaderIfNeeded() { }

void ensureDailyHeader(const char* filename) {
  if (!SD.exists(filename)) {
    File f = SD.open(filename, FILE_WRITE);
    if (f) {
      f.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");
      f.close();
    }
  }
}

void appendCsvData() {
  extern unsigned long currentEpoch;
  int year, month, day, hour, minute, second, weekday;
  epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);

  // Build shared date/time strings
  char dateStr[12]; snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month + 1, day);
  char timeStr[9];  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", hour, minute, second);
  char ymd[7];      snprintf(ymd, sizeof(ymd), "%02d%02d%02d", year % 100, month + 1, day);

  // --- Aging Room ---
  char tFile[13]; snprintf(tFile, sizeof(tFile), "%s_T.csv", ymd);
  char hFile[13]; snprintf(hFile, sizeof(hFile), "%s_H.csv", ymd);
  ensureDailyHeader(tFile);
  ensureDailyHeader(hFile);

  extern float tA, tB, tC, tD, hA, hB, hC, hD;

  File tf = SD.open(tFile, FILE_WRITE);
  if (tf) {
    char buf[8];
    tf.print(dateStr); tf.print(","); tf.print(timeStr); tf.print(",");
    if (isnan(tA)) { tf.print("ERR"); } else { dtostrf(tA, 4, 1, buf); tf.print(buf); tf.print(" C"); } tf.print(",");
    if (isnan(tB)) { tf.print("ERR"); } else { dtostrf(tB, 4, 1, buf); tf.print(buf); tf.print(" C"); } tf.print(",");
    if (isnan(tC)) { tf.print("ERR"); } else { dtostrf(tC, 4, 1, buf); tf.print(buf); tf.print(" C"); } tf.print(",");
    if (isnan(tD)) { tf.print("ERR"); } else { dtostrf(tD, 4, 1, buf); tf.print(buf); tf.print(" C"); } tf.println();
    tf.close(); wdt_reset();
  }

  File hf = SD.open(hFile, FILE_WRITE);
  if (hf) {
    char buf[8];
    hf.print(dateStr); hf.print(","); hf.print(timeStr); hf.print(",");
    if (isnan(hA)) { hf.print("ERR"); } else { dtostrf(hA, 4, 1, buf); hf.print(buf); hf.print(" %"); } hf.print(",");
    if (isnan(hB)) { hf.print("ERR"); } else { dtostrf(hB, 4, 1, buf); hf.print(buf); hf.print(" %"); } hf.print(",");
    if (isnan(hC)) { hf.print("ERR"); } else { dtostrf(hC, 4, 1, buf); hf.print(buf); hf.print(" %"); } hf.print(",");
    if (isnan(hD)) { hf.print("ERR"); } else { dtostrf(hD, 4, 1, buf); hf.print(buf); hf.print(" %"); } hf.println();
    hf.close(); wdt_reset();
  }

  // --- Skit Room ---
  extern float tSkit, hSkit;
  char skTFile[13]; snprintf(skTFile, sizeof(skTFile), "%sST.csv", ymd);
  char skHFile[13]; snprintf(skHFile, sizeof(skHFile), "%sSH.csv", ymd);

  if (!SD.exists(skTFile)) {
    File f = SD.open(skTFile, FILE_WRITE);
    if (f) { f.println("Date,Time,Skit"); f.close(); }
  }

  File skT = SD.open(skTFile, FILE_WRITE);
  if (skT) {
    char buf[8];
    skT.print(dateStr); skT.print(","); skT.print(timeStr); skT.print(",");
    if (isnan(tSkit)) { skT.print("ERR"); } else { dtostrf(tSkit, 4, 1, buf); skT.print(buf); skT.print(" C"); }
    skT.println(); skT.close(); wdt_reset();
  }

  File skH = SD.open(skHFile, FILE_WRITE);
  if (skH) {
    char buf[8];
    skH.print(dateStr); skH.print(","); skH.print(timeStr); skH.print(",");
    if (isnan(hSkit)) { skH.print("ERR"); } else { dtostrf(hSkit, 4, 1, buf); skH.print(buf); skH.print(" %"); }
    skH.println(); skH.close(); wdt_reset();
  }

  // --- Camera Room ---
  extern float tCam, hCam;
  char caTFile[13]; snprintf(caTFile, sizeof(caTFile), "%sCT.csv", ymd);
  char caHFile[13]; snprintf(caHFile, sizeof(caHFile), "%sCH.csv", ymd);

  if (!SD.exists(caTFile)) {
    File f = SD.open(caTFile, FILE_WRITE);
    if (f) { f.println("Date,Time,Camera"); f.close(); }
  }
  if (!SD.exists(caHFile)) {
    File f = SD.open(caHFile, FILE_WRITE);
    if (f) { f.println("Date,Time,Camera"); f.close(); }
  }

  File caT = SD.open(caTFile, FILE_WRITE);
  if (caT) {
    char buf[8];
    caT.print(dateStr); caT.print(","); caT.print(timeStr); caT.print(",");
    if (isnan(tCam)) { caT.print("ERR"); } else { dtostrf(tCam, 4, 1, buf); caT.print(buf); caT.print(" C"); }
    caT.println(); caT.close(); wdt_reset();
  }

  File caH = SD.open(caHFile, FILE_WRITE);
  if (caH) {
    char buf[8];
    caH.print(dateStr); caH.print(","); caH.print(timeStr); caH.print(",");
    if (isnan(hCam)) { caH.print("ERR"); } else { dtostrf(hCam, 4, 1, buf); caH.print(buf); caH.print(" %"); }
    caH.println(); caH.close(); wdt_reset();
  }
}

void purgeOldLogs() {
  extern unsigned long currentEpoch;
  if (currentEpoch < 1000000000UL) return;

  unsigned long purgeEpoch = currentEpoch - (180UL * 86400UL);
  int y, mo, d, h, mi, s, wd;
  epochToDateTime(purgeEpoch, y, mo, d, h, mi, s, wd);

  char ymd[7]; snprintf(ymd, sizeof(ymd), "%02d%02d%02d", y % 100, mo + 1, d);

  // Aging Room
  char tFile[13]; snprintf(tFile, sizeof(tFile), "%s_T.csv", ymd);
  char hFile[13]; snprintf(hFile, sizeof(hFile), "%s_H.csv", ymd);
  if (SD.exists(tFile)) SD.remove(tFile);
  if (SD.exists(hFile)) SD.remove(hFile);

  // Skit Room
  char skT[13]; snprintf(skT, sizeof(skT), "%sST.csv", ymd);
  char skH[13]; snprintf(skH, sizeof(skH), "%sSH.csv", ymd);
  if (SD.exists(skT)) SD.remove(skT);
  if (SD.exists(skH)) SD.remove(skH);

  // Camera Room
  char caT[13]; snprintf(caT, sizeof(caT), "%sCT.csv", ymd);
  char caH[13]; snprintf(caH, sizeof(caH), "%sCH.csv", ymd);
  if (SD.exists(caT)) SD.remove(caT);
  if (SD.exists(caH)) SD.remove(caH);
}

void serveFile(EthernetClient &client, const char *filename, const char *contentType) {
  if (strcmp(filename, "EVENTS.txt") == 0 && !SD.exists(filename)) {
    client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\n");
    return;
  }

  if (strcmp(filename, "temp.csv") == 0 || strcmp(filename, "humid.csv") == 0 ||
      strcmp(filename, "SK_T.csv") == 0 || strcmp(filename, "SK_H.csv") == 0 ||
      strcmp(filename, "CA_T.csv") == 0 || strcmp(filename, "CA_H.csv") == 0) {

    // Determine file prefix and type
    bool isTemp  = (strcmp(filename, "temp.csv") == 0 || strcmp(filename, "SK_T.csv") == 0 || strcmp(filename, "CA_T.csv") == 0);
    bool isAging = (strcmp(filename, "temp.csv") == 0 || strcmp(filename, "humid.csv") == 0);
    bool isSkit  = (strcmp(filename, "SK_T.csv") == 0 || strcmp(filename, "SK_H.csv") == 0);
    // Camera is the remaining case

    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: "); client.println(contentType);
    client.print("Content-Disposition: inline; filename=\""); client.print(filename); client.println("\"");
    client.println("Connection: close\n");

    // Print appropriate header row
    if (isAging) client.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");
    else if (isSkit) client.println("Date,Time,Skit");
    else client.println("Date,Time,Camera");

    extern unsigned long currentEpoch;
    for (int i = 6; i >= 0; i--) {
      unsigned long targetEpoch = currentEpoch - (i * 86400UL);
      int y, mo, d, h, mi, s, wd;
      epochToDateTime(targetEpoch, y, mo, d, h, mi, s, wd);
      char ymd[7]; snprintf(ymd, sizeof(ymd), "%02d%02d%02d", y % 100, mo + 1, d);

      char fn[13];
      if      (isAging && isTemp)  snprintf(fn, sizeof(fn), "%s_T.csv", ymd);
      else if (isAging && !isTemp) snprintf(fn, sizeof(fn), "%s_H.csv", ymd);
      else if (isSkit  && isTemp)  snprintf(fn, sizeof(fn), "%sST.csv", ymd);
      else if (isSkit  && !isTemp) snprintf(fn, sizeof(fn), "%sSH.csv", ymd);
      else if (isTemp)             snprintf(fn, sizeof(fn), "%sCT.csv", ymd);
      else                         snprintf(fn, sizeof(fn), "%sCH.csv", ymd);

      if (SD.exists(fn)) {
        File f = SD.open(fn, FILE_READ);
        if (f) {
          while(f.available()) { wdt_reset(); if (f.read() == '\n') break; }
          byte buf[128];
          while(f.available()) {
            int n = f.read(buf, sizeof(buf));
            client.write(buf, n);
            wdt_reset();
          }
          f.close();
        }
      }
    }
    return;
  }

  if (SD.exists(filename)) {
    File file = SD.open(filename, FILE_READ);
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: "); client.println(contentType);
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
}

void serveThreshold(EthernetClient &client) {
  extern float tempThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  client.println(tempThreshold, 1);
}

void serveStatus(EthernetClient &client) {
  extern float tA, tB, tC, tD;
  extern float hA, hB, hC, hD;
  extern float tempThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  const char* labels[] = {"A", "B", "C", "D"};
  float temps[] = {tA, tB, tC, tD};
  float humids[] = {hA, hB, hC, hD};
  
  for (int i = 0; i < 4; i++) {
    if (i > 0) client.print(",");
    client.print(labels[i]); client.print(":");
    
    if (isnan(temps[i])) { client.print("ERR|ERR"); } 
    else {
      client.print(temps[i], 1); client.print("|");
      if (temps[i] < tempThreshold - THRESHOLD_MARGIN) client.print("LOW");
      else if (temps[i] > tempThreshold + THRESHOLD_MARGIN) client.print("HIGH");
      else client.print("OK");
    }
    client.print("|");
    if (isnan(humids[i])) { client.print("ERR"); }
    else { client.print(humids[i], 1); }
  }
  client.println();
}

void serveSystemInfo(EthernetClient &client) {
  extern unsigned long lastNtpEpoch;
  extern unsigned long lastCsvWrite;

  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  int ram = freeMemory();
  client.print("RAM:"); client.print(ram); client.print(",");

  unsigned long ms = millis();
  unsigned long secs = ms / 1000;
  unsigned long mins = secs / 60;
  unsigned long hours = mins / 60;
  unsigned long days = hours / 24;
  client.print("UPTIME:"); client.print(days); client.print("d ");
  client.print(hours % 24); client.print("h "); client.print(mins % 60); client.print("m,");

  // Use cached sdReady flag — avoids calling SD.begin() on every poll cycle
  client.print("SD:"); client.print(sdReady ? "OK" : "FAIL"); client.print(",");

  if (lastCsvWrite == 0) { client.print("LASTWRITE:Never"); } 
  else {
    extern unsigned long currentEpoch;
    unsigned long secsAgo = (millis() - lastCsvWrite) / 1000;
    unsigned long writeEpoch = currentEpoch - secsAgo;
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(writeEpoch, y, mo, d, h, mi, s, wd);
    char buf[20]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print("LASTWRITE:"); client.print(buf);
  }
  client.print(",");

  if (lastNtpEpoch == 0) { client.print("NTPSYNC:Never"); } 
  else {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(lastNtpEpoch, y, mo, d, h, mi, s, wd);
    char buf[20]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print("NTPSYNC:"); client.print(buf);
  }
  client.println();
}

void serveRootPage(EthernetClient &client) {
  extern unsigned long currentEpoch;
  int _y, _mo, _d, _h, _mi, _s, _wd;
  epochToDateTime(currentEpoch, _y, _mo, _d, _h, _mi, _s, _wd);
  char lastUpdate[20];
  snprintf(lastUpdate, sizeof(lastUpdate), "%04d-%02d-%02d %02d:%02d:%02d", _y, _mo + 1, _d, _h, _mi, _s);
  extern float tempThreshold;
  extern float tA, tB, tC, tD;

  client.println(F("HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: close\n"));
  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<title>Seegrid Aging Room Data</title>"));
  client.println(F("<style>"));
  client.println(F("body{font-family:sans-serif;background:#f4f4f4;padding:12px 16px;box-sizing:border-box;margin:0;}"));
  client.println(F(".tab{display:inline-block;padding:10px 20px;margin:5px;background:#ccc;cursor:pointer;border-radius:4px;}"));
  client.println(F(".tab.active{background:#999;color:#fff;font-weight:bold;}"));
  client.println(F(".tab-content{display:none;} .tab-content.active{display:block;}"));
  client.println(F("button{margin-left:10px;padding:6px 12px;cursor:pointer;}"));
  client.println(F(".chart-scroll-wrapper{width:100%;background:#fff;border-radius:6px;padding:10px;box-sizing:border-box;}"));
  client.println(F(".chart-scroll-wrapper canvas{height:500px !important;display:block;}"));
  client.println(F("#statusBar{display:flex;gap:20px;align-items:center;padding:12px 16px;margin-bottom:12px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:15px;flex-wrap:wrap;}"));
  client.println(F(".sensor-dot{display:inline-block;width:14px;height:14px;border-radius:50%;margin-right:6px;vertical-align:middle;}"));
  client.println(F("@keyframes errorBlink { 0% { opacity: 1; } 50% { opacity: 0; } 100% { opacity: 1; } }"));
  client.println(F(".dot-warn{background:#f39c12;} .dot-err{background:#e74c3c; animation: errorBlink 1s infinite;}"));
  client.println(F(".sensor-label{font-weight:bold;} .sensor-warn{color:#f39c12;} .sensor-err{color:#e74c3c; animation: errorBlink 1s infinite;}"));
  client.println(F(".sensor-temp{color:#555;font-size:14px;margin-left:4px;}"));
  
  wdt_reset(); 

  client.println(F(".sensor-dot.s-a{background:#0072B2;} .sensor-label.s-a{color:#0072B2;}"));
  client.println(F(".sensor-dot.s-b{background:#E69F00;} .sensor-label.s-b{color:#E69F00;}"));
  client.println(F(".sensor-dot.s-c{background:#CC79A7;} .sensor-label.s-c{color:#CC79A7;}"));
  client.println(F(".sensor-dot.s-d{background:#56B4E9;} .sensor-label.s-d{color:#56B4E9;}"));
  client.println(F(".sensor-dot.s-avg{background:#000;} .sensor-label.s-avg{color:#000; font-weight:bold;}"));
  client.println(F("</style>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns/dist/chartjs-adapter-date-fns.bundle.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/hammerjs@2.0.8/hammer.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@1.2.1/dist/chartjs-plugin-zoom.min.js'></script>"));
  client.println(F("</head><body>"));

  client.println(F("<div style='display:flex;gap:10px;margin-bottom:16px;'>"));
  client.println(F("<a style='padding:8px 16px;background:#1a5276;color:white;border-radius:4px;font-size:14px;text-decoration:none;'>Aging Room</a>"));
  client.println(F("<a href='/skit' style='padding:8px 16px;background:#2980b9;color:white;border-radius:4px;font-size:14px;text-decoration:none;'>Skit Room</a>"));
  client.println(F("<a href='/camera' style='padding:8px 16px;background:#2980b9;color:white;border-radius:4px;font-size:14px;text-decoration:none;'>Camera Room</a>"));
  client.println(F("</div>"));

  client.println(F("<h2>Seegrid Aging Room Data</h2>"));
  client.println(F("<div style='margin-bottom: 20px; font-size: 14px; color: #555;'>"));
  client.print(F("Last update: <strong id='lastUpdate' style='color: #000;'>"));
  client.print(lastUpdate);
  client.println(F("</strong></div>"));

  client.println(F("<div id='statusBar'><strong>Sensors:</strong>"));
  const char* sensorLabels[] = {"A", "B", "C", "D"};
  const char* sensorIdClasses[] = {"s-a", "s-b", "s-c", "s-d"};
  float initTemps[] = {tA, tB, tC, tD};
  
  for (int i = 0; i < 4; i++) {
    wdt_reset(); 
    bool isErr = isnan(initTemps[i]);
    bool isLow = !isErr && initTemps[i] < tempThreshold - THRESHOLD_MARGIN;
    bool isHigh = !isErr && initTemps[i] > tempThreshold + THRESHOLD_MARGIN;
    bool isOk = !isErr && !isLow && !isHigh;
    client.print(F("  <span id='statusSensor")); client.print(sensorLabels[i]);
    client.print(F("'><span class='sensor-dot ")); client.print(sensorIdClasses[i]);
    if (isErr) client.print(F(" dot-err")); else if (!isOk) client.print(F(" dot-warn"));
    client.print(F("'></span><span class='sensor-label ")); client.print(sensorIdClasses[i]);
    if (isErr) client.print(F(" sensor-err")); else if (!isOk) client.print(F(" sensor-warn"));
    client.print(F("'>")); client.print(sensorLabels[i]);
    if (isErr) { client.print(F(" ERR")); } else {
      float tC_val = initTemps[i]; float tF = tC_val * 9.0 / 5.0 + 32.0;
      client.print(F("</span><span class='sensor-temp'>")); client.print(tC_val, 1);
      client.print(F("°C (")); client.print(tF, 1); client.print(F("°F)"));
      if (isLow) client.print(F(" ↓ LOW")); else if (isHigh) client.print(F(" ↑ HIGH")); else client.print(F(" ✓"));
    }
    client.print(F("</span></span>  "));
  }
  // AVG entry — always present, content updated by JS based on active tab
  client.println(F("  <span id='statusSensorAVG'><span class='sensor-dot s-avg'></span><span class='sensor-label s-avg'>AVG</span><span class='sensor-temp' id='avgTemp'></span></span>"));
  client.println(F("</div>"));

  client.println(F("<div>"));
  client.println(F("<div class='tab active' onclick=\"showTab('temp', event)\">Temperature</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('humid', event)\">Humidity</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('archive', event)\">Archive Data</div>"));
  client.println(F("</div>"));

  wdt_reset(); 

  client.println(F("<div id='temp' class='tab-content active'>"));
  client.println(F("<label>Range: <select id='tempRange'><option value='1' selected>1</option><option value='3'>3</option><option value='5'>5</option><option value='7'>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(tempChart, \"temp\")'>Export PNG</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(tempChart&&tempChart.resetZoom)tempChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><br><div class='chart-scroll-wrapper'><canvas id='tempChart'></canvas></div></div>"));

  client.println(F("<div id='humid' class='tab-content'>"));
  client.println(F("<label>Range: <select id='humidRange'><option value='1' selected>1</option><option value='3'>3</option><option value='5'>5</option><option value='7'>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(humidChart, \"humid\")'>Export PNG</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(humidChart&&humidChart.resetZoom)humidChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><br><div class='chart-scroll-wrapper'><canvas id='humidChart'></canvas></div></div>"));

  client.println(F("<div id='archive' class='tab-content'>"));
  client.println(F("  <div style='background:#fff; padding:20px; border-radius:6px; border:1px solid #ddd; margin-top:10px;'>"));
  client.println(F("    <h3 style='margin-top:0;'>Download Historical Data Range (Up to 6 Months)</h3>"));
  client.println(F("    <p>Select a date range to generate a combined CSV report. Data is strictly retained for 180 days.</p>"));
  client.println(F("    <label><b>From:</b> <input type='date' id='startDate' style='padding:6px; border:1px solid #ccc; border-radius:4px;'></label>"));
  client.println(F("    <label style='margin-left: 15px;'><b>To:</b> <input type='date' id='endDate' style='padding:6px; border:1px solid #ccc; border-radius:4px;'></label><br><br>"));
  client.println(F("    <label><b>Data Type:</b> <select id='dataType' style='padding:6px; border:1px solid #ccc; border-radius:4px;'><option value='T'>Temperature</option><option value='H'>Humidity</option></select></label><br><br>"));
  client.println(F("    <button id='dlButton' onclick='downloadDateRange()' style='padding:10px 16px; font-weight:bold; background:#2980b9; color:white; border:none; border-radius:4px; cursor:pointer;'>Download Combined CSV</button>"));
  client.println(F("    <span id='dlProgress' style='margin-left: 10px; font-weight: bold; color: #27ae60;'></span>"));
  client.println(F("  </div></div>"));

  wdt_reset(); 

  client.println(F("<div id='sysPanel' style='margin-top:16px;padding:12px 16px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:14px;'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:8px;font-size:15px;display:flex;justify-content:space-between;'><span>System Status</span>"));
  client.println(F("<button onclick='safeEject()' style='background:#e74c3c; color:white; border:none; padding:4px 8px; border-radius:4px; font-weight:bold; cursor:pointer;'>Prepare SD for Removal / Halt</button></div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;'>"));
  client.println(F("<span id='sysRam'>RAM: --</span> <span id='sysUptime'>&#9201; Uptime: --</span> <span id='sysSd'>&#128190; SD: --</span>"));
  client.println(F("</div><div style='display:flex;flex-wrap:wrap;gap:20px;margin-top:6px;'>"));
  client.println(F("<span id='sysWrite'>&#128221; Last Write: --</span> <span id='sysNtp'>&#128336; NTP Sync: --</span>"));
  client.println(F("</div></div>"));

  client.println(F("<div id='alertsPanel' style='margin-top:16px;padding:12px 16px;background:#fff;border-radius:6px;border:1px solid #f5c6cb;background-color:#f8d7da;font-size:14px;'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:8px;font-size:15px;color:#721c24;display:flex;justify-content:space-between;align-items:center;'>"));
  client.println(F("<span>&#9888;&#65039; Recent Watchdog Alerts</span>"));
  client.println(F("<button onclick='clearEvents()' style='background:#dc3545;color:white;border:none;padding:4px 10px;border-radius:4px;font-weight:bold;cursor:pointer;font-size:13px;'>Clear Alerts</button></div>"));
  client.println(F("<ul id='eventList' style='margin:0; padding-left:20px; color:#721c24; font-family:monospace;'><li>Loading alerts...</li></ul>"));
  client.println(F("</div>"));

  client.println(F("<script>"));
  client.println(F("let tempChart, humidChart; let activeTab = 'temp'; let lastStatus = [];"));
  client.print(F("let threshold = ")); client.print(tempThreshold, 1); client.println(F("; const margin = 5.0;"));

  wdt_reset(); 

  client.println(F("let isOffline = false; let failedPings = 0;"));
  client.println(F("const warnBanner = document.createElement('div');"));
  client.println(F("warnBanner.style.cssText = 'display:none; background:#f1c40f; color:#856404; padding:15px; text-align:center; font-weight:bold; font-size:16px; position:fixed; top:0; left:0; width:100%; z-index:9999; box-shadow:0 4px 6px rgba(0,0,0,0.1);';"));
  client.println(F("warnBanner.innerHTML = '&#9888;&#65039; SYSTEM OFFLINE: Connection lost. Auto-reconnecting...';"));
  client.println(F("document.body.prepend(warnBanner);"));

  client.println(F("function handleDisconnect() {"));
  client.println(F("  failedPings++;"));
  client.println(F("  if (failedPings >= 3) { if(isOffline) return; isOffline = true; warnBanner.style.display = 'block'; setTimeout(checkReconnect, 4000); }"));
  client.println(F("}"));

  client.println(F("async function checkReconnect() {"));
  client.println(F("  try { let r = await fetch('/status?t=' + new Date().getTime());"));
  client.println(F("    if(r.ok || r.status === 401) { isOffline = false; failedPings = 0; warnBanner.style.display = 'none'; pollStatus(); pollSysInfo(); }"));
  client.println(F("    else setTimeout(checkReconnect, 4000);"));
  client.println(F("  } catch(e) { setTimeout(checkReconnect, 4000); }"));
  client.println(F("}"));

  client.println(F("function showTab(id, evt){"));
  client.println(F("  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));"));
  client.println(F("  document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));"));
  client.println(F("  document.getElementById(id).classList.add('active'); evt.target.classList.add('active');"));
  client.println(F("  activeTab = id; if (lastStatus.length > 0) updateStatusBar(lastStatus);"));
  client.println(F("}"));

  client.println(F("function downloadChart(chart, label){"));
  client.println(F("  if(!chart) return; const link = document.createElement('a');"));
  client.println(F("  link.download = label + '_chart.png'; link.href = chart.toBase64Image(); link.click();"));
  client.println(F("}"));

  wdt_reset(); 

  client.println(F("async function downloadDateRange() {"));
  client.println(F("  if (isOffline) return;"));
  client.println(F("  const startDate = document.getElementById('startDate').value; const endDate = document.getElementById('endDate').value;"));
  client.println(F("  const dataType = document.getElementById('dataType').value; const btn = document.getElementById('dlButton'); const progress = document.getElementById('dlProgress');"));
  client.println(F("  if (!startDate || !endDate) { alert('Please select Start and End date.'); return; }"));
  client.println(F("  const d1 = new Date(startDate + 'T12:00:00'); const d2 = new Date(endDate + 'T12:00:00');"));
  client.println(F("  if (d1 > d2) { alert('Start date must be BEFORE End date.'); return; }"));
  client.println(F("  const diffDays = Math.round((d2 - d1) / 86400000);"));
  client.println(F("  if (diffDays > 180) { alert('Max 6 months allowed.'); return; }"));
  
  client.println(F("  btn.disabled = true; btn.style.backgroundColor = '#95a5a6';"));
  client.println(F("  let combinedCsv = 'Date,Time,Sensor A,Sensor B,Sensor C,Sensor D\\n'; let filesFound = 0;"));
  
  client.println(F("  for (let i = 0; i <= diffDays; i++) {"));
  client.println(F("    if (isOffline) break;"));
  client.println(F("    let curr = new Date(d1.getTime() + (i * 86400000));"));
  client.println(F("    let y = curr.getFullYear().toString().slice(-2); let m = (curr.getMonth() + 1).toString().padStart(2, '0'); let d = curr.getDate().toString().padStart(2, '0');"));
  client.println(F("    let filename = y + m + d + '_' + dataType + '.csv'; progress.innerText = 'Fetching day ' + (i + 1) + ' of ' + (diffDays + 1) + '...';"));
  client.println(F("    try { let res = await safeFetch('/archive?file=' + filename); if (res && res.ok) { let text = await res.text(); let lines = text.trim().split('\\n');"));
  client.println(F("    if (lines.length > 1) { combinedCsv += lines.slice(1).join('\\n') + '\\n'; filesFound++; } } } catch(e) { console.log('Skip ' + filename); }"));
  client.println(F("    await new Promise(resolve => setTimeout(resolve, 100));"));
  client.println(F("  }"));
  
  client.println(F("  btn.disabled = false; btn.style.backgroundColor = '#2980b9'; progress.innerText = 'Done!'; setTimeout(() => progress.innerText = '', 3000);"));
  client.println(F("  if (filesFound === 0) { alert('No data found.'); return; }"));
  client.println(F("  const blob = new Blob([combinedCsv], { type: 'text/csv' }); const url = window.URL.createObjectURL(blob);"));
  client.println(F("  const a = document.createElement('a'); a.setAttribute('href', url);"));
  client.println(F("  a.setAttribute('download', 'AgingRoom_' + dataType + '_' + startDate + '_to_' + endDate + '.csv');"));
  client.println(F("  a.click(); window.URL.revokeObjectURL(url);"));
  client.println(F("}"));

  wdt_reset(); 

  client.println(F("async function safeEject() { if(confirm('STOP LOGGING AND UNMOUNT SD CARD?\\nYou MUST physically unplug and reboot the Arduino to resume logging.')) { try { await fetch('/eject'); document.body.innerHTML = '<div style=\"text-align:center;margin-top:100px;\"><h1 style=\"color:#e74c3c;\">System Halted Safely</h1><p>The SD card has been unmounted.</p><p>You may now safely unplug the power.</p></div>'; } catch(e) { handleDisconnect(); } } }"));

  client.println(F("const sensorMap = { 'A':{idClass:'s-a', color:'#0072B2'}, 'B':{idClass:'s-b', color:'#E69F00'}, 'C':{idClass:'s-c', color:'#CC79A7'}, 'D':{idClass:'s-d', color:'#56B4E9'} };"));

  client.println(F("function updateStatusBar(records) {"));
  client.println(F("  lastStatus = records;"));
  client.println(F("  let sumT = 0, sumH = 0, cntT = 0, cntH = 0;"));
  client.println(F("  records.forEach(rec => {"));
  client.println(F("    const colonIdx = rec.indexOf(':'); if (colonIdx === -1) return;"));
  client.println(F("    const label = rec.substring(0, colonIdx).trim(); const parts = rec.substring(colonIdx + 1).trim().split('|');"));
  client.println(F("    const tempC = parts[0], state = parts[1], humid = parts.length > 2 ? parts[2] : null;"));
  client.println(F("    const isErr = state === 'ERR', isOk = state === 'OK', isLow = state === 'LOW', isHigh = state === 'HIGH';"));
  client.println(F("    const info = sensorMap[label]; if (!info) return;"));
  client.println(F("    const el = document.getElementById('statusSensor' + label);"));
  client.println(F("    if (el) {"));
  client.println(F("      el.querySelector('.sensor-dot').className = 'sensor-dot ' + info.idClass + (isErr ? ' dot-err' : !isOk ? ' dot-warn' : '');"));
  client.println(F("      const lbl = el.querySelector('.sensor-label'); lbl.className = 'sensor-label ' + info.idClass + (isErr ? ' sensor-err' : !isOk ? ' sensor-warn' : '');"));
  client.println(F("      if (isErr) { lbl.textContent = label + ' ERR'; const ste = el.querySelector('.sensor-temp'); if (ste) ste.textContent = ''; }"));
  client.println(F("      else {"));
  client.println(F("        lbl.textContent = label;"));
  client.println(F("        const cVal = parseFloat(tempC), fVal = (cVal * 9 / 5 + 32).toFixed(1);"));
  client.println(F("        const hVal = parseFloat(humid);"));
  client.println(F("        if (!isNaN(cVal)) { sumT += cVal; cntT++; }"));
  client.println(F("        if (!isNaN(hVal)) { sumH += hVal; cntH++; }"));
  client.println(F("        if (activeTab === 'humid') {"));
  client.println(F("          el.querySelector('.sensor-temp').textContent = hVal.toFixed(1) + '% RH';"));
  client.println(F("        } else if (activeTab === 'archive') {"));
  client.println(F("          el.querySelector('.sensor-temp').textContent = cVal.toFixed(1) + '°C (' + fVal + '°F) ' + hVal.toFixed(1) + '% RH';"));
  client.println(F("        } else {"));
  client.println(F("          const suffix = isLow ? ' \u2193 LOW' : isHigh ? ' \u2191 HIGH' : ' \u2713';"));
  client.println(F("          el.querySelector('.sensor-temp').textContent = cVal.toFixed(1) + '°C (' + fVal + '°F)' + suffix;"));
  client.println(F("        }"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("    if (tempChart) { const ds = tempChart.data.datasets.find(d => d.label === 'Sensor ' + label); if (ds) { ds.borderColor = isErr ? '#e74c3c' : info.color; ds.backgroundColor = ds.borderColor; ds.pointBackgroundColor = ds.borderColor; } }"));
  client.println(F("  });"));
  client.println(F("  const avgEl = document.getElementById('avgTemp');"));
  client.println(F("  if (avgEl) {"));
  client.println(F("    const avgT = cntT > 0 ? (sumT / cntT) : null;"));
  client.println(F("    const avgH = cntH > 0 ? (sumH / cntH) : null;"));
  client.println(F("    if (activeTab === 'humid') {"));
  client.println(F("      avgEl.textContent = avgH !== null ? avgH.toFixed(1) + '% RH' : '--';"));
  client.println(F("    } else if (activeTab === 'archive') {"));
  client.println(F("      const avgF = avgT !== null ? (avgT * 9 / 5 + 32).toFixed(1) : null;"));
  client.println(F("      avgEl.textContent = (avgT !== null ? avgT.toFixed(1) + '°C (' + avgF + '°F) ' : '--') + (avgH !== null ? avgH.toFixed(1) + '% RH' : '--');"));
  client.println(F("    } else {"));
  client.println(F("      const avgF = avgT !== null ? (avgT * 9 / 5 + 32).toFixed(1) : null;"));
  client.println(F("      avgEl.textContent = avgT !== null ? avgT.toFixed(1) + '°C (' + avgF + '°F)' : '--';"));
  client.println(F("    }"));
  client.println(F("  }"));
  client.println(F("  if (tempChart) tempChart.update();"));
  client.println(F("}"));

  wdt_reset(); 

  client.println(F("async function safeFetch(url) {"));
  client.println(F("  if (isOffline && !url.includes('/status')) return null;"));
  client.println(F("  try { let res = await fetch(url); if (!res.ok && res.status !== 404 && res.status !== 401) throw new Error('Bad Network'); failedPings = 0; return res; }"));
  client.println(F("  catch (e) { handleDisconnect(); return null; }"));
  client.println(F("}"));

  client.println(F("async function fetchData(filename, rangeDays) {"));
  client.println(F("  let res = await safeFetch('/' + filename + '?t=' + new Date().getTime());"));
  client.println(F("  if (!res || !res.ok) return {labels:[], sensorsA:[], sensorsB:[], sensorsC:[], sensorsD:[], avgData:[]};"));
  client.println(F("  let text = await res.text(); let lines = text.trim().split('\\n').slice(1);"));
  client.println(F("  let limit = new Date().getTime() - rangeDays * 86400000;"));
  client.println(F("  let labels=[], sensorsA=[], sensorsB=[], sensorsC=[], sensorsD=[], avgData=[];"));
  client.println(F("  let downsampleRate = rangeDays <= 1 ? 1 : rangeDays <= 3 ? 6 : 12;"));
  client.println(F("  lines.forEach((line, idx) => {"));
  client.println(F("    if (idx % downsampleRate !== 0) return;"));
  client.println(F("    let [date, time, a, b, c, d] = line.split(','); if (!date || !time) return;"));
  client.println(F("    let dtStr = date.split('-').join('/') + ' ' + time; let dt = new Date(dtStr);"));
  client.println(F("    if(dt.getTime() >= limit){"));
  client.println(F("      const va=parseFloat(a), vb=parseFloat(b), vc=parseFloat(c), vd=parseFloat(d);"));
  client.println(F("      const vals=[va,vb,vc,vd].filter(v=>!isNaN(v));"));
  client.println(F("      const avg = vals.length > 0 ? vals.reduce((s,v)=>s+v,0)/vals.length : null;"));
  client.println(F("      labels.push(dt.getTime()); sensorsA.push(isNaN(va)?null:va); sensorsB.push(isNaN(vb)?null:vb); sensorsC.push(isNaN(vc)?null:vc); sensorsD.push(isNaN(vd)?null:vd); avgData.push(avg);"));
  client.println(F("    }"));
  client.println(F("  });"));
  client.println(F("  return {labels, sensorsA, sensorsB, sensorsC, sensorsD, avgData};"));
  client.println(F("}"));

  client.println(F("async function pollThreshold() { if(isOffline) return; try { let res = await safeFetch('/threshold?t=' + new Date().getTime()); if(res){ let val = parseFloat(await res.text()); if (!isNaN(val) && val !== threshold) { threshold = val; updateThresholdLines(); } } } catch(e) { handleDisconnect(); } }"));
  client.println(F("async function pollStatus() { if(isOffline) return; try { let res = await safeFetch('/status?t=' + new Date().getTime()); if(res){ let text = await res.text(); updateStatusBar(text.trim().split(',')); } } catch(e) { handleDisconnect(); } }"));

  client.println(F("function updateThresholdLines() {"));
  client.println(F("  if (tempChart) { let len = tempChart.data.labels.length;"));
  client.println(F("    tempChart.data.datasets[5].data = Array(len).fill(threshold);"));
  client.println(F("    tempChart.data.datasets[6].data = Array(len).fill(threshold + margin);"));
  client.println(F("    tempChart.data.datasets[7].data = Array(len).fill(threshold - margin);"));
  client.println(F("    tempChart.update();"));
  client.println(F("  }"));
  client.println(F("}"));

  wdt_reset(); 

  client.println(F("async function updateCharts(){"));
  client.println(F("  if (isOffline) return;"));
  client.println(F("  let rangeT = parseInt(document.getElementById('tempRange').value); let rangeH = parseInt(document.getElementById('humidRange').value);"));
  
  client.println(F("  let timeFmtT = rangeT > 1 ? 'MMM d, HH:mm' : 'HH:mm'; let timeFmtH = rangeH > 1 ? 'MMM d, HH:mm' : 'HH:mm';"));
  
  client.println(F("  let tempData = await fetchData('temp.csv', rangeT); let humidData = await fetchData('humid.csv', rangeH);"));

  client.println(F("  if (tempData.labels.length > 0) {"));
  client.println(F("    if(tempChart) tempChart.destroy();"));
  client.println(F("    tempChart = new Chart(document.getElementById('tempChart'), {"));
  client.println(F("      type: 'line', data: { labels: tempData.labels, datasets: ["));
  client.println(F("        {label: 'Sensor A', data: tempData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: tempData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: tempData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: tempData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'AVG', data: tempData.avgData, borderColor: '#000', backgroundColor: '#000', fill: false, borderWidth: 3, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Threshold', data: Array(tempData.labels.length).fill(threshold), borderColor: 'black', borderDash: [5,5], pointRadius: 0},"));
  client.println(F("        {label: 'High Threshold', data: Array(tempData.labels.length).fill(threshold + margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0},"));
  client.println(F("        {label: 'Low Threshold', data: Array(tempData.labels.length).fill(threshold - margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0}"));
  client.println(F("      ]},"));
  
  client.println(F("      options: { responsive: true, maintainAspectRatio: false, layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: { x: { type: 'time', time: { tooltipFormat: 'yyyy-MM-dd HH:mm', displayFormats: { hour: timeFmtT, minute: timeFmtT, day: 'MMM d' } }, ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } }, grid: { color: c => (c.tick && c.tick.value && new Date(c.tick.value).getHours()===0 && new Date(c.tick.value).getMinutes()===0) ? 'rgba(0,0,0,0.6)' : 'rgba(0,0,0,0.1)', lineWidth: c => (c.tick && c.tick.value && new Date(c.tick.value).getHours()===0 && new Date(c.tick.value).getMinutes()===0) ? 2 : 1 } },"));
  
  client.println(F("      y: { title: { display: true, text: 'Celsius (°C)', font: { size: 13 } }, ticks: { stepSize: 1.0 } } },"));
  client.println(F("      interaction: { mode: 'index', intersect: false }, plugins: { tooltip: { mode: 'index', intersect: false },"));
  client.println(F("      legend: { labels: { boxWidth: 24, padding: 16, font: { size: 13 } }, onClick: function(e, legendItem, legend) { const index = legendItem.datasetIndex; const ci = legend.chart; if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); } } },"));
  client.println(F("      zoom: typeof ChartZoom !== 'undefined' ? { pan: { enabled: true, mode: 'x' }, zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' } } : {} } }"));
  client.println(F("    });"));
  client.println(F("  }"));

  wdt_reset(); 

  client.println(F("  if (humidData.labels.length > 0) {"));
  client.println(F("    if(humidChart) humidChart.destroy();"));
  client.println(F("    humidChart = new Chart(document.getElementById('humidChart'), {"));
  client.println(F("      type: 'line', data: { labels: humidData.labels, datasets: ["));
  client.println(F("        {label: 'Sensor A', data: humidData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: humidData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: humidData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: humidData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'AVG', data: humidData.avgData, borderColor: '#000', backgroundColor: '#000', fill: false, borderWidth: 3, pointRadius: 0, pointHoverRadius: 4}"));
  client.println(F("      ]},"));
  
  client.println(F("      options: { responsive: true, maintainAspectRatio: false, layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: { x: { type: 'time', time: { tooltipFormat: 'yyyy-MM-dd HH:mm', displayFormats: { hour: timeFmtH, minute: timeFmtH, day: 'MMM d' } }, ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } }, grid: { color: c => (c.tick && c.tick.value && new Date(c.tick.value).getHours()===0 && new Date(c.tick.value).getMinutes()===0) ? 'rgba(0,0,0,0.6)' : 'rgba(0,0,0,0.1)', lineWidth: c => (c.tick && c.tick.value && new Date(c.tick.value).getHours()===0 && new Date(c.tick.value).getMinutes()===0) ? 2 : 1 } },"));
  
  client.println(F("      y: { title: { display: true, text: 'Humidity (%)', font: { size: 13 } }, ticks: { stepSize: 1.0 } } },"));
  client.println(F("      interaction: { mode: 'index', intersect: false }, plugins: { tooltip: { mode: 'index', intersect: false },"));
  client.println(F("      legend: { labels: { boxWidth: 24, padding: 16, font: { size: 13 } }, onClick: function(e, legendItem, legend) { const index = legendItem.datasetIndex; const ci = legend.chart; if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); } } },"));
  client.println(F("      zoom: typeof ChartZoom !== 'undefined' ? { pan: { enabled: true, mode: 'x' }, zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' } } : {} } }"));
  client.println(F("    });"));
  client.println(F("  }"));

  client.println(F("  setTimeout(pollStatus, 500); updateLastUpdate();"));
  client.println(F("}"));

  wdt_reset(); 

  client.println(F("async function pollSysInfo() { if(isOffline) return; try { const res = await safeFetch('/sysinfo?t=' + new Date().getTime()); if(!res) return; const text = await res.text(); const pairs = {};"));
  client.println(F("  text.trim().split(',').forEach(p => { const i = p.indexOf(':'); if (i !== -1) pairs[p.substring(0,i).trim()] = p.substring(i+1).trim(); });"));
  client.println(F("  const ram = parseInt(pairs['RAM'] || 0); const ramColor = ram > 2000 ? '#2ecc71' : ram > 1000 ? '#f39c12' : '#e74c3c';"));
  client.println(F("  const ramEl = document.getElementById('sysRam'); if (ramEl) ramEl.innerHTML = 'RAM: <span style=\"color:' + ramColor + '; font-weight:bold;\">' + (ram/1024).toFixed(1) + ' KB</span>';"));
  client.println(F("  const upEl = document.getElementById('sysUptime'); if (upEl) upEl.innerHTML = '&#9201; Uptime: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['UPTIME'] || '--') + '</span>';"));
  client.println(F("  const sdEl = document.getElementById('sysSd'); if (sdEl) { sdEl.textContent = 'SD: ' + (pairs['SD'] || '--'); sdEl.style.color = pairs['SD']==='OK' ? '#27ae60' : '#e74c3c'; }"));
  client.println(F("  const wrEl = document.getElementById('sysWrite'); if (wrEl) wrEl.innerHTML = '&#128221; Last Write: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['LASTWRITE'] || '--') + '</span>';"));
  client.println(F("  const ntpEl = document.getElementById('sysNtp'); if (ntpEl) ntpEl.innerHTML = '&#128336; NTP Sync: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['NTPSYNC'] || '--') + '</span>';"));
  client.println(F("} catch(e) { handleDisconnect(); } }"));
  
  client.println(F("function updateLastUpdate() { const n=new Date(); const pad=v=>String(v).padStart(2,'0'); document.getElementById('lastUpdate').textContent = n.getFullYear()+'-'+pad(n.getMonth()+1)+'-'+pad(n.getDate())+' '+pad(n.getHours())+':'+pad(n.getMinutes())+':'+pad(n.getSeconds()); }"));

  client.println(F("async function clearEvents() { if (confirm('Clear alert history?')) { try { await fetch('/clear-events'); document.getElementById('eventList').innerHTML = '<li>No recent alerts.</li>'; } catch(e) { handleDisconnect(); } } }"));
  
  client.println(F("async function pollEvents() { if(isOffline) return; try { let res = await safeFetch('/events?t=' + new Date().getTime()); if (res && res.ok) { let text = await res.text(); let lines = text.trim().split('\\n').filter(l => l.trim().length > 0); let last5 = lines.slice(-5).reverse(); let html = ''; if (last5.length === 0) { html = '<li>No recent alerts.</li>'; } else { last5.forEach(l => html += '<li>' + l + '</li>'); } document.getElementById('eventList').innerHTML = html; } else if (res) { document.getElementById('eventList').innerHTML = '<li>No alert log found on SD card yet.</li>'; } } catch(e) { handleDisconnect(); } }"));

  client.println(F("async function bootUp() {"));
  client.println(F("  document.getElementById('tempRange').addEventListener('change', updateCharts);"));
  client.println(F("  document.getElementById('humidRange').addEventListener('change', updateCharts);"));

  client.println(F("  try {"));
  client.println(F("    await updateCharts();"));
  client.println(F("    await new Promise(r => setTimeout(r, 500));"));
  client.println(F("    await pollStatus();"));
  client.println(F("    await new Promise(r => setTimeout(r, 500));"));
  client.println(F("    await pollSysInfo();"));
  client.println(F("    await new Promise(r => setTimeout(r, 500));"));
  client.println(F("    await pollThreshold();"));
  client.println(F("    await new Promise(r => setTimeout(r, 500));"));
  client.println(F("    await pollEvents();"));
  client.println(F("  } catch(e) { console.log('Bootup interrupted, but UI active'); }"));

  client.println(F("  setInterval(updateCharts, 307000);"));
  client.println(F("  setTimeout(function(){ setInterval(pollStatus,    29000); }, 5000);"));
  client.println(F("  setTimeout(function(){ setInterval(pollSysInfo,   31000); }, 10000);"));
  client.println(F("  setTimeout(function(){ setInterval(pollThreshold, 37000); }, 15000);"));
  client.println(F("  setTimeout(function(){ setInterval(pollEvents,    53000); }, 20000);"));
  client.println(F("}"));

  client.println(F("setTimeout(bootUp, 2000);"));
  client.println(F("</script></body></html>"));
}

// ============================================================
// SKIT ROOM — Simple endpoint handlers
// ============================================================

void serveSkitStatus(EthernetClient &client) {
  extern float tSkit, hSkit, skitTempThreshold, skitHumidThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  if (isnan(tSkit)) { client.print("TEMP:ERR|ERR"); }
  else {
    client.print("TEMP:"); client.print(tSkit, 1); client.print("|");
    if      (tSkit < skitTempThreshold - THRESHOLD_MARGIN) client.print("LOW");
    else if (tSkit > skitTempThreshold + THRESHOLD_MARGIN) client.print("HIGH");
    else client.print("OK");
  }
  client.print(",");
  if (isnan(hSkit)) { client.print("HUMID:ERR|ERR"); }
  else {
    client.print("HUMID:"); client.print(hSkit, 1); client.print("|");
    if      (hSkit < skitHumidThreshold - THRESHOLD_MARGIN) client.print("LOW");
    else if (hSkit > skitHumidThreshold + THRESHOLD_MARGIN) client.print("HIGH");
    else client.print("OK");
  }
  client.println();
}

void serveSkitThresholdTemp(EthernetClient &client) {
  extern float skitTempThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  client.println(skitTempThreshold, 1);
}

void serveSkitThresholdHumid(EthernetClient &client) {
  extern float skitHumidThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  client.println(skitHumidThreshold, 1);
}

void serveSkitSysInfo(EthernetClient &client) {
  extern unsigned long lastNtpEpoch, lastCsvWrite, currentEpoch;
  extern unsigned long lastSkitReceive;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  int ram = freeMemory();
  client.print("RAM:"); client.print(ram); client.print(",");
  unsigned long ms = millis();
  unsigned long mins = (ms/1000)/60, hours = mins/60, days = hours/24;
  client.print("UPTIME:"); client.print(days); client.print("d ");
  client.print(hours%24); client.print("h "); client.print(mins%60); client.print("m,");
  client.print("SD:"); client.print(sdReady ? "OK" : "FAIL"); client.print(",");
  if (lastSkitReceive == 0) { client.print("LASTRECEIVE:Never"); }
  else {
    unsigned long secsAgo = (millis() - lastSkitReceive) / 1000;
    unsigned long recvEpoch = currentEpoch - secsAgo;
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(recvEpoch, y, mo, d, h, mi, s, wd);
    char buf[20]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print("LASTRECEIVE:"); client.print(buf);
  }
  client.print(",");
  if (lastNtpEpoch == 0) { client.print("NTPSYNC:Never"); }
  else {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(lastNtpEpoch, y, mo, d, h, mi, s, wd);
    char buf[20]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print("NTPSYNC:"); client.print(buf);
  }
  client.println();
}

// ============================================================
// CAMERA ROOM — Simple endpoint handlers
// ============================================================

void serveCameraStatus(EthernetClient &client) {
  extern float tCam, hCam, camTempThreshold, camHumidThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  if (isnan(tCam)) { client.print("TEMP:ERR|ERR"); }
  else {
    client.print("TEMP:"); client.print(tCam, 1); client.print("|");
    if      (tCam < camTempThreshold - THRESHOLD_MARGIN) client.print("LOW");
    else if (tCam > camTempThreshold + THRESHOLD_MARGIN) client.print("HIGH");
    else client.print("OK");
  }
  client.print(",");
  if (isnan(hCam)) { client.print("HUMID:ERR|ERR"); }
  else {
    client.print("HUMID:"); client.print(hCam, 1); client.print("|");
    if      (hCam < camHumidThreshold - THRESHOLD_MARGIN) client.print("LOW");
    else if (hCam > camHumidThreshold + THRESHOLD_MARGIN) client.print("HIGH");
    else client.print("OK");
  }
  client.println();
}

void serveCameraThresholdTemp(EthernetClient &client) {
  extern float camTempThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  client.println(camTempThreshold, 1);
}

void serveCameraThresholdHumid(EthernetClient &client) {
  extern float camHumidThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  client.println(camHumidThreshold, 1);
}

void serveCameraSysInfo(EthernetClient &client) {
  extern unsigned long lastNtpEpoch, lastCsvWrite, currentEpoch;
  extern unsigned long lastCamReceive;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  int ram = freeMemory();
  client.print("RAM:"); client.print(ram); client.print(",");
  unsigned long ms = millis();
  unsigned long mins = (ms/1000)/60, hours = mins/60, days = hours/24;
  client.print("UPTIME:"); client.print(days); client.print("d ");
  client.print(hours%24); client.print("h "); client.print(mins%60); client.print("m,");
  client.print("SD:"); client.print(sdReady ? "OK" : "FAIL"); client.print(",");
  if (lastCamReceive == 0) { client.print("LASTRECEIVE:Never"); }
  else {
    unsigned long secsAgo = (millis() - lastCamReceive) / 1000;
    unsigned long recvEpoch = currentEpoch - secsAgo;
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(recvEpoch, y, mo, d, h, mi, s, wd);
    char buf[20]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print("LASTRECEIVE:"); client.print(buf);
  }
  client.print(",");
  if (lastNtpEpoch == 0) { client.print("NTPSYNC:Never"); }
  else {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(lastNtpEpoch, y, mo, d, h, mi, s, wd);
    char buf[20]; snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print("NTPSYNC:"); client.print(buf);
  }
  client.println();
}


// ============================================================
// ROOM PAGE — Consolidated dashboard for Skit and Camera rooms
// roomName    : "Skit Room" or "Camera Room"
// urlBase     : "/skit" or "/camera"
// filePrefix  : "SK" or "CA"
// tempColor   : chart line color for temperature dataset
// humidColor  : chart line color for humidity dataset
// dlPrefix    : "Skit" or "Camera" (download filename prefix)
// tThresh     : current temp threshold value
// hThresh     : current humid threshold value
// activeNav   : which nav button is active ("skit" or "camera")
// ============================================================
static void serveRoomPage(EthernetClient &client,
                          const char* roomName,
                          const char* urlBase,
                          const char* filePrefix,
                          const char* tempColor,
                          const char* humidColor,
                          const char* dlPrefix,
                          float tThresh,
                          float hThresh,
                          const char* activeNav) {
  extern unsigned long currentEpoch;
  int _y, _mo, _d, _h, _mi, _s, _wd;
  epochToDateTime(currentEpoch, _y, _mo, _d, _h, _mi, _s, _wd);
  char lastUpdate[20];
  snprintf(lastUpdate, sizeof(lastUpdate), "%04d-%02d-%02d %02d:%02d:%02d", _y, _mo+1, _d, _h, _mi, _s);

  // Build room-specific URL strings once
  char statusUrl[24], sysinfoUrl[26], threshTUrl[32], threshHUrl[34];
  char tempCsvUrl[24], humidCsvUrl[26];
  snprintf(statusUrl,   sizeof(statusUrl),   "%s/status",         urlBase);
  snprintf(sysinfoUrl,  sizeof(sysinfoUrl),  "%s/sysinfo",        urlBase);
  snprintf(threshTUrl,  sizeof(threshTUrl),  "%s/threshold/temp", urlBase);
  snprintf(threshHUrl,  sizeof(threshHUrl),  "%s/threshold/humid",urlBase);
  snprintf(tempCsvUrl,  sizeof(tempCsvUrl),  "%s/temp.csv",       urlBase);
  snprintf(humidCsvUrl, sizeof(humidCsvUrl), "%s/humid.csv",      urlBase);

  client.println(F("HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: close\n"));
  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.print(F("<title>")); client.print(roomName); client.println(F("</title>"));
  client.println(F("<style>"));
  client.println(F("body{font-family:sans-serif;background:#f4f4f4;padding:12px 16px;box-sizing:border-box;margin:0;}"));
  client.println(F(".tab{display:inline-block;padding:10px 20px;margin:5px;background:#ccc;cursor:pointer;border-radius:4px;}"));
  client.println(F(".tab.active{background:#999;color:#fff;font-weight:bold;}"));
  client.println(F(".tab-content{display:none;} .tab-content.active{display:block;}"));
  client.println(F("button{margin-left:10px;padding:6px 12px;cursor:pointer;}"));
  client.println(F(".chart-scroll-wrapper{width:100%;background:#fff;border-radius:6px;padding:10px;box-sizing:border-box;}"));
  client.println(F(".chart-scroll-wrapper canvas{height:500px !important;display:block;}"));
  client.println(F("#statusBar{display:flex;gap:20px;align-items:center;padding:12px 16px;margin-bottom:12px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:15px;flex-wrap:wrap;}"));
  client.println(F(".nav-bar{display:flex;gap:10px;margin-bottom:16px;}"));
  client.println(F(".nav-btn{padding:8px 16px;background:#2980b9;color:white;border:none;border-radius:4px;cursor:pointer;font-size:14px;text-decoration:none;}"));
  client.println(F(".nav-btn.active-room{background:#1a5276;}"));
  client.println(F(".thresh-panel{background:#fff;border-radius:6px;border:1px solid #ddd;padding:12px 16px;margin-top:16px;font-size:14px;}"));
  client.println(F("</style>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns/dist/chartjs-adapter-date-fns.bundle.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/hammerjs@2.0.8/hammer.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@1.2.1/dist/chartjs-plugin-zoom.min.js'></script>"));
  client.println(F("</head><body>"));

  wdt_reset();

  // Navigation bar — highlight active room
  client.println(F("<div class='nav-bar'>"));
  client.println(F("<a class='nav-btn' href='/'>Aging Room</a>"));
  client.print(F("<a class='nav-btn"));
  if (strcmp(activeNav, "skit") == 0) client.print(F(" active-room"));
  client.println(F("' href='/skit'>Skit Room</a>"));
  client.print(F("<a class='nav-btn"));
  if (strcmp(activeNav, "camera") == 0) client.print(F(" active-room"));
  client.println(F("' href='/camera'>Camera Room</a>"));
  client.println(F("</div>"));

  client.print(F("<h2>")); client.print(roomName); client.println(F("</h2>"));
  client.println(F("<div style='margin-bottom:20px;font-size:14px;color:#555;'>"));
  client.print(F("Last update: <strong id='lastUpdate' style='color:#000;'>"));
  client.print(lastUpdate);
  client.println(F("</strong></div>"));

  client.println(F("<div id='statusBar'><strong>Sensor:</strong>"));
  client.println(F("<span id='statusTemp'>Temp: --</span>"));
  client.println(F("<span id='statusHumid'>Humid: --</span>"));
  client.println(F("</div>"));

  client.println(F("<div>"));
  client.println(F("<div class='tab active' onclick=\"showTab('temp',event)\">Temperature</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('humid',event)\">Humidity</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('archive',event)\">Archive Data</div>"));
  client.println(F("</div>"));

  wdt_reset();

  client.println(F("<div id='temp' class='tab-content active'>"));
  client.println(F("<label>Range: <select id='tempRange'><option value='1' selected>1</option><option value='3'>3</option><option value='5'>5</option><option value='7'>7</option></select> days</label>"));
  client.println(F("<button onclick='if(tempChart){const l=document.createElement(\"a\");l.download=\"temp_chart.png\";l.href=tempChart.toBase64Image();l.click();}'>Export PNG</button>"));
  client.println(F("<button onclick='if(typeof updateCharts===\"function\")updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(tempChart&&tempChart.resetZoom)tempChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><br><div class='chart-scroll-wrapper'><canvas id='tempChart'></canvas></div></div>"));

  client.println(F("<div id='humid' class='tab-content'>"));
  client.println(F("<label>Range: <select id='humidRange'><option value='1' selected>1</option><option value='3'>3</option><option value='5'>5</option><option value='7'>7</option></select> days</label>"));
  client.println(F("<button onclick='if(humidChart){const l=document.createElement(\"a\");l.download=\"humid_chart.png\";l.href=humidChart.toBase64Image();l.click();}'>Export PNG</button>"));
  client.println(F("<button onclick='if(typeof updateCharts===\"function\")updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(humidChart&&humidChart.resetZoom)humidChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><br><div class='chart-scroll-wrapper'><canvas id='humidChart'></canvas></div></div>"));

  client.println(F("<div id='archive' class='tab-content'>"));
  client.println(F("  <div style='background:#fff;padding:20px;border-radius:6px;border:1px solid #ddd;margin-top:10px;'>"));
  client.println(F("    <h3 style='margin-top:0;'>Download Historical Data (Up to 6 Months)</h3>"));
  client.println(F("    <label><b>From:</b> <input type='date' id='startDate' style='padding:6px;border:1px solid #ccc;border-radius:4px;'></label>"));
  client.println(F("    <label style='margin-left:15px;'><b>To:</b> <input type='date' id='endDate' style='padding:6px;border:1px solid #ccc;border-radius:4px;'></label><br><br>"));
  client.println(F("    <label><b>Data Type:</b> <select id='dataType' style='padding:6px;border:1px solid #ccc;border-radius:4px;'><option value='T'>Temperature</option><option value='H'>Humidity</option></select></label><br><br>"));
  client.println(F("    <button id='dlButton' onclick='downloadDateRange()' style='padding:10px 16px;font-weight:bold;background:#2980b9;color:white;border:none;border-radius:4px;cursor:pointer;'>Download CSV</button>"));
  client.println(F("    <span id='dlProgress' style='margin-left:10px;font-weight:bold;color:#27ae60;'></span>"));
  client.println(F("  </div></div>"));

  wdt_reset();

  client.println(F("<div id='sysPanel' style='margin-top:16px;padding:12px 16px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:14px;'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:8px;font-size:15px;'>System Status</div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;'>"));
  client.println(F("<span id='sysRam'>RAM: --</span> <span id='sysUptime'>&#9201; Uptime: --</span> <span id='sysSd'>&#128190; SD: --</span>"));
  client.println(F("</div><div style='display:flex;flex-wrap:wrap;gap:20px;margin-top:6px;'>"));
  client.println(F("<span id='sysReceive'>&#128225; Last Receive: --</span> <span id='sysNtp'>&#128336; NTP Sync: --</span>"));
  client.println(F("</div></div>"));

  client.println(F("<div class='thresh-panel'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:10px;font-size:15px;'>Threshold Adjustment</div>"));
  client.println(F("<div style='display:flex;gap:30px;flex-wrap:wrap;'>"));
  client.println(F("<div><b>Temperature</b><br>"));
  client.println(F("<button onclick='adjustThresh(\"temp\",-1)'>&#9660;</button>"));
  client.println(F("<span id='threshTempVal' style='margin:0 10px;font-size:16px;font-weight:bold;'>--</span>°C"));
  client.println(F("<button onclick='adjustThresh(\"temp\",1)'>&#9650;</button></div>"));
  client.println(F("<div><b>Humidity</b><br>"));
  client.println(F("<button onclick='adjustThresh(\"humid\",-1)'>&#9660;</button>"));
  client.println(F("<span id='threshHumidVal' style='margin:0 10px;font-size:16px;font-weight:bold;'>--</span>%"));
  client.println(F("<button onclick='adjustThresh(\"humid\",1)'>&#9650;</button></div>"));
  client.println(F("</div></div>"));

  wdt_reset();

  // JavaScript — inject room-specific values as JS variables, rest is generic
  client.println(F("<script>"));
  client.println(F("let tempChart, humidChart;"));
  client.print(F("let tempThresh=")); client.print(tThresh, 1); client.println(F(";"));
  client.print(F("let humidThresh=")); client.print(hThresh, 1); client.println(F(";"));
  client.println(F("const margin=5.0;"));
  client.print(F("const STATUS_URL='")); client.print(statusUrl); client.println(F("';"));
  client.print(F("const SYSINFO_URL='")); client.print(sysinfoUrl); client.println(F("';"));
  client.print(F("const THRESH_T_URL='")); client.print(threshTUrl); client.println(F("';"));
  client.print(F("const THRESH_H_URL='")); client.print(threshHUrl); client.println(F("';"));
  client.print(F("const TEMP_CSV_URL='")); client.print(tempCsvUrl); client.println(F("';"));
  client.print(F("const HUMID_CSV_URL='")); client.print(humidCsvUrl); client.println(F("';"));
  client.print(F("const TEMP_COLOR='")); client.print(tempColor); client.println(F("';"));
  client.print(F("const HUMID_COLOR='")); client.print(humidColor); client.println(F("';"));
  client.print(F("const FILE_PREFIX='")); client.print(filePrefix); client.println(F("';"));
  client.print(F("const DL_PREFIX='")); client.print(dlPrefix); client.println(F("';"));
  client.print(F("const ROOM_NAME='")); client.print(roomName); client.println(F("';"));
  client.println(F("let isOffline=false; let failedPings=0;"));

  client.println(F("const warnBanner=document.createElement('div');"));
  client.println(F("warnBanner.style.cssText='display:none;background:#f1c40f;color:#856404;padding:15px;text-align:center;font-weight:bold;font-size:16px;position:fixed;top:0;left:0;width:100%;z-index:9999;';"));
  client.println(F("warnBanner.innerHTML='&#9888;&#65039; SYSTEM OFFLINE: Connection lost. Auto-reconnecting...';"));
  client.println(F("document.body.prepend(warnBanner);"));

  client.println(F("function handleDisconnect(){failedPings++;if(failedPings>=3){if(isOffline)return;isOffline=true;warnBanner.style.display='block';setTimeout(checkReconnect,4000);}}"));
  client.println(F("async function checkReconnect(){try{let r=await fetch(STATUS_URL+'?t='+Date.now());if(r.ok||r.status===401){isOffline=false;failedPings=0;warnBanner.style.display='none';pollStatus();pollSysInfo();}else setTimeout(checkReconnect,4000);}catch(e){setTimeout(checkReconnect,4000);}}"));
  client.println(F("async function safeFetch(url){if(isOffline&&!url.includes('/status'))return null;try{let r=await fetch(url);if(!r.ok&&r.status!==404&&r.status!==401)throw new Error('Bad');failedPings=0;return r;}catch(e){handleDisconnect();return null;}}"));
  client.println(F("function showTab(id,evt){document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));document.getElementById(id).classList.add('active');evt.target.classList.add('active');}"));

  wdt_reset();

  client.println(F("async function pollStatus(){if(isOffline)return;try{let r=await safeFetch(STATUS_URL+'?t='+Date.now());if(!r)return;let txt=await r.text();"));
  client.println(F("  let parts=txt.trim().split(',');"));
  client.println(F("  parts.forEach(p=>{let ci=p.indexOf(':');if(ci===-1)return;let key=p.substring(0,ci),val=p.substring(ci+1).split('|');"));
  client.println(F("    if(key==='TEMP'){let v=parseFloat(val[0]),st=val[1];let tEl=document.getElementById('statusTemp');"));
  client.println(F("      if(st==='ERR'){tEl.innerHTML='<b>Temp: ERR</b>';}else{let f=(v*9/5+32).toFixed(1);let col=st==='OK'?'#27ae60':'#e74c3c';tEl.innerHTML='Temp: <b style=\"color:'+col+'\">'+v.toFixed(1)+'°C ('+f+'°F) '+st+'</b>';}}"));
  client.println(F("    if(key==='HUMID'){let v=parseFloat(val[0]),st=val[1];let hEl=document.getElementById('statusHumid');"));
  client.println(F("      if(st==='ERR'){hEl.innerHTML='<b>Humid: ERR</b>';}else{let col=st==='OK'?'#27ae60':'#e74c3c';hEl.innerHTML='Humid: <b style=\"color:'+col+'\">'+v.toFixed(1)+'% RH '+st+'</b>';}}"));
  client.println(F("  });"));
  client.println(F("}catch(e){handleDisconnect();}}"));

  client.println(F("async function pollThresholds(){try{"));
  client.println(F("  let rt=await safeFetch(THRESH_T_URL+'?t='+Date.now());if(rt){let v=parseFloat(await rt.text());if(!isNaN(v)){tempThresh=v;document.getElementById('threshTempVal').textContent=v.toFixed(1);}}"));
  client.println(F("  let rh=await safeFetch(THRESH_H_URL+'?t='+Date.now());if(rh){let v=parseFloat(await rh.text());if(!isNaN(v)){humidThresh=v;document.getElementById('threshHumidVal').textContent=v.toFixed(1);}}"));
  client.println(F("}catch(e){}}"));

  client.println(F("async function adjustThresh(type,delta){"));
  client.println(F("  if(type==='temp'){tempThresh=Math.round((tempThresh+delta)*10)/10;document.getElementById('threshTempVal').textContent=tempThresh.toFixed(1);await fetch(THRESH_T_URL+'?v='+tempThresh,{method:'POST'});}"));
  client.println(F("  else{humidThresh=Math.round((humidThresh+delta)*10)/10;document.getElementById('threshHumidVal').textContent=humidThresh.toFixed(1);await fetch(THRESH_H_URL+'?v='+humidThresh,{method:'POST'});}"));
  client.println(F("  updateChartThreshLines();"));
  client.println(F("}"));

  client.println(F("function updateChartThreshLines(){"));
  client.println(F("  if(tempChart){let l=tempChart.data.labels.length;tempChart.data.datasets[1].data=Array(l).fill(tempThresh);tempChart.data.datasets[2].data=Array(l).fill(tempThresh+margin);tempChart.data.datasets[3].data=Array(l).fill(tempThresh-margin);tempChart.update();}"));
  client.println(F("  if(humidChart){let l=humidChart.data.labels.length;humidChart.data.datasets[1].data=Array(l).fill(humidThresh);humidChart.data.datasets[2].data=Array(l).fill(humidThresh+margin);humidChart.data.datasets[3].data=Array(l).fill(humidThresh-margin);humidChart.update();}"));
  client.println(F("}"));

  wdt_reset();

  client.println(F("async function fetchData(csvUrl,rangeDays){"));
  client.println(F("  let r=await safeFetch(csvUrl+'?t='+Date.now());if(!r||!r.ok)return{labels:[],vals:[]};"));
  client.println(F("  let txt=await r.text();let lines=txt.trim().split('\\n').slice(1);"));
  client.println(F("  let limit=Date.now()-rangeDays*86400000;let labels=[],vals=[];"));
  client.println(F("  let ds=rangeDays<=1?1:rangeDays<=3?6:12;"));
  client.println(F("  lines.forEach((line,idx)=>{if(idx%ds!==0)return;let[date,time,v]=line.split(',');if(!date||!time)return;"));
  client.println(F("    let dt=new Date(date.split('-').join('/')+' '+time);"));
  client.println(F("    if(dt.getTime()>=limit){labels.push(dt.getTime());vals.push(parseFloat(v)||null);}"));
  client.println(F("  });return{labels,vals};"));
  client.println(F("}"));

  client.println(F("async function updateCharts(){if(isOffline)return;"));
  client.println(F("  let rT=parseInt(document.getElementById('tempRange').value);"));
  client.println(F("  let rH=parseInt(document.getElementById('humidRange').value);"));
  client.println(F("  let fmtT=rT>1?'MMM d, HH:mm':'HH:mm';let fmtH=rH>1?'MMM d, HH:mm':'HH:mm';"));
  client.println(F("  let td=await fetchData(TEMP_CSV_URL,rT);let hd=await fetchData(HUMID_CSV_URL,rH);"));

  client.println(F("  if(td.labels.length>0){if(tempChart)tempChart.destroy();"));
  client.println(F("    tempChart=new Chart(document.getElementById('tempChart'),{type:'line',data:{labels:td.labels,datasets:["));
  client.print(F("      {label:ROOM_NAME+' Temp',data:td.vals,borderColor:TEMP_COLOR,backgroundColor:TEMP_COLOR,fill:false,borderWidth:2,pointRadius:0,pointHoverRadius:4},"));
  client.println(F(""));
  client.println(F("      {label:'Threshold',data:Array(td.labels.length).fill(tempThresh),borderColor:'black',borderDash:[5,5],pointRadius:0},"));
  client.println(F("      {label:'High',data:Array(td.labels.length).fill(tempThresh+margin),borderColor:'gray',borderDash:[2,2],pointRadius:0},"));
  client.println(F("      {label:'Low',data:Array(td.labels.length).fill(tempThresh-margin),borderColor:'gray',borderDash:[2,2],pointRadius:0}"));
  client.println(F("    ]},options:{responsive:true,maintainAspectRatio:false,"));
  client.println(F("    scales:{x:{type:'time',time:{tooltipFormat:'yyyy-MM-dd HH:mm',displayFormats:{hour:fmtT,minute:fmtT,day:'MMM d'}},ticks:{maxRotation:45,minRotation:45,maxTicksLimit:24,font:{size:10}},grid:{color:c=>(c.tick&&c.tick.value&&new Date(c.tick.value).getHours()===0&&new Date(c.tick.value).getMinutes()===0)?'rgba(0,0,0,0.6)':'rgba(0,0,0,0.1)',lineWidth:c=>(c.tick&&c.tick.value&&new Date(c.tick.value).getHours()===0&&new Date(c.tick.value).getMinutes()===0)?2:1}},"));
  client.println(F("    y:{title:{display:true,text:'Celsius (°C)',font:{size:13}},ticks:{stepSize:1.0}}},"));
  client.println(F("    interaction:{mode:'index',intersect:false},plugins:{tooltip:{mode:'index',intersect:false},legend:{labels:{boxWidth:24,padding:16,font:{size:13}}},"));
  client.println(F("    zoom:typeof ChartZoom!=='undefined'?{pan:{enabled:true,mode:'x'},zoom:{wheel:{enabled:true},pinch:{enabled:true},mode:'x'}}:{}}}});"));
  client.println(F("  }"));

  wdt_reset();

  client.println(F("  if(hd.labels.length>0){if(humidChart)humidChart.destroy();"));
  client.println(F("    humidChart=new Chart(document.getElementById('humidChart'),{type:'line',data:{labels:hd.labels,datasets:["));
  client.print(F("      {label:ROOM_NAME+' Humid',data:hd.vals,borderColor:HUMID_COLOR,backgroundColor:HUMID_COLOR,fill:false,borderWidth:2,pointRadius:0,pointHoverRadius:4},"));
  client.println(F(""));
  client.println(F("      {label:'Threshold',data:Array(hd.labels.length).fill(humidThresh),borderColor:'black',borderDash:[5,5],pointRadius:0},"));
  client.println(F("      {label:'High',data:Array(hd.labels.length).fill(humidThresh+margin),borderColor:'gray',borderDash:[2,2],pointRadius:0},"));
  client.println(F("      {label:'Low',data:Array(hd.labels.length).fill(humidThresh-margin),borderColor:'gray',borderDash:[2,2],pointRadius:0}"));
  client.println(F("    ]},options:{responsive:true,maintainAspectRatio:false,"));
  client.println(F("    scales:{x:{type:'time',time:{tooltipFormat:'yyyy-MM-dd HH:mm',displayFormats:{hour:fmtH,minute:fmtH,day:'MMM d'}},ticks:{maxRotation:45,minRotation:45,maxTicksLimit:24,font:{size:10}},grid:{color:c=>(c.tick&&c.tick.value&&new Date(c.tick.value).getHours()===0&&new Date(c.tick.value).getMinutes()===0)?'rgba(0,0,0,0.6)':'rgba(0,0,0,0.1)',lineWidth:c=>(c.tick&&c.tick.value&&new Date(c.tick.value).getHours()===0&&new Date(c.tick.value).getMinutes()===0)?2:1}},"));
  client.println(F("    y:{title:{display:true,text:'Humidity (%)',font:{size:13}},ticks:{stepSize:1.0}}},"));
  client.println(F("    interaction:{mode:'index',intersect:false},plugins:{tooltip:{mode:'index',intersect:false},legend:{labels:{boxWidth:24,padding:16,font:{size:13}}},"));
  client.println(F("    zoom:typeof ChartZoom!=='undefined'?{pan:{enabled:true,mode:'x'},zoom:{wheel:{enabled:true},pinch:{enabled:true},mode:'x'}}:{}}}});"));
  client.println(F("  }"));
  client.println(F("  updateLastUpdate();"));
  client.println(F("}"));

  client.println(F("async function pollSysInfo(){if(isOffline)return;try{const r=await safeFetch(SYSINFO_URL+'?t='+Date.now());if(!r)return;const txt=await r.text();const p={};"));
  client.println(F("  txt.trim().split(',').forEach(x=>{const i=x.indexOf(':');if(i!==-1)p[x.substring(0,i).trim()]=x.substring(i+1).trim();});"));
  client.println(F("  const ram=parseInt(p['RAM']||0);const rc=ram>2000?'#2ecc71':ram>1000?'#f39c12':'#e74c3c';"));
  client.println(F("  const re=document.getElementById('sysRam');if(re)re.innerHTML='RAM: <span style=\"color:'+rc+';font-weight:bold;\">'+(ram/1024).toFixed(1)+' KB</span>';"));
  client.println(F("  const ue=document.getElementById('sysUptime');if(ue)ue.innerHTML='&#9201; Uptime: <span style=\"color:#27ae60;font-weight:bold;\">'+(p['UPTIME']||'--')+'</span>';"));
  client.println(F("  const se=document.getElementById('sysSd');if(se){se.textContent='SD: '+(p['SD']||'--');se.style.color=p['SD']==='OK'?'#27ae60':'#e74c3c';}"));
  client.println(F("  const lre=document.getElementById('sysReceive');if(lre)lre.innerHTML='&#128225; Last Receive: <span style=\"color:#27ae60;font-weight:bold;\">'+(p['LASTRECEIVE']||'--')+'</span>';"));
  client.println(F("  const ne=document.getElementById('sysNtp');if(ne)ne.innerHTML='&#128336; NTP Sync: <span style=\"color:#27ae60;font-weight:bold;\">'+(p['NTPSYNC']||'--')+'</span>';"));
  client.println(F("}catch(e){handleDisconnect();}}"));

  client.println(F("async function downloadDateRange(){if(isOffline)return;"));
  client.println(F("  const s=document.getElementById('startDate').value,e=document.getElementById('endDate').value,dt=document.getElementById('dataType').value;"));
  client.println(F("  if(!s||!e){alert('Select dates.');return;}const d1=new Date(s+'T12:00:00'),d2=new Date(e+'T12:00:00');"));
  client.println(F("  if(d1>d2){alert('Start must be before End.');return;}const diff=Math.round((d2-d1)/86400000);if(diff>180){alert('Max 6 months.');return;}"));
  client.println(F("  const btn=document.getElementById('dlButton'),prog=document.getElementById('dlProgress');"));
  client.println(F("  btn.disabled=true;let csv='Date,Time,Value\\n',found=0;"));
  client.println(F("  for(let i=0;i<=diff;i++){let c=new Date(d1.getTime()+i*86400000);"));
  client.println(F("    let y=c.getFullYear().toString().slice(-2),m=(c.getMonth()+1).toString().padStart(2,'0'),d=c.getDate().toString().padStart(2,'0');"));
  client.println(F("    let fn=y+m+d+FILE_PREFIX+(dt==='T'?'T':'H')+'.csv';prog.innerText='Fetching '+(i+1)+'/'+(diff+1)+'...';"));
  client.println(F("    try{let r=await safeFetch('/archive?file='+fn);if(r&&r.ok){let t=await r.text();let l=t.trim().split('\\n');if(l.length>1){csv+=l.slice(1).join('\\n')+'\\n';found++;}}}catch(e){}"));
  client.println(F("    await new Promise(res=>setTimeout(res,100));"));
  client.println(F("  }"));
  client.println(F("  btn.disabled=false;prog.innerText='Done!';setTimeout(()=>prog.innerText='',3000);"));
  client.println(F("  if(!found){alert('No data found.');return;}"));
  client.println(F("  const blob=new Blob([csv],{type:'text/csv'});const url=URL.createObjectURL(blob);"));
  client.println(F("  const a=document.createElement('a');a.href=url;a.download=DL_PREFIX+'_'+dt+'_'+s+'_to_'+e+'.csv';a.click();URL.revokeObjectURL(url);"));
  client.println(F("}"));

  client.println(F("function updateLastUpdate(){const n=new Date();const pad=v=>String(v).padStart(2,'0');document.getElementById('lastUpdate').textContent=n.getFullYear()+'-'+pad(n.getMonth()+1)+'-'+pad(n.getDate())+' '+pad(n.getHours())+':'+pad(n.getMinutes())+':'+pad(n.getSeconds());}"));

  client.println(F("async function bootUp(){"));
  client.println(F("  document.getElementById('tempRange').addEventListener('change',updateCharts);"));
  client.println(F("  document.getElementById('humidRange').addEventListener('change',updateCharts);"));
  client.println(F("  try{"));
  client.println(F("    await updateCharts();"));
  client.println(F("    await new Promise(r=>setTimeout(r,500));"));
  client.println(F("    await pollStatus();"));
  client.println(F("    await new Promise(r=>setTimeout(r,500));"));
  client.println(F("    await pollSysInfo();"));
  client.println(F("    await new Promise(r=>setTimeout(r,500));"));
  client.println(F("    await pollThresholds();"));
  client.println(F("  }catch(e){}"));
  client.println(F("  setInterval(updateCharts,307000);"));
  client.println(F("  setTimeout(function(){setInterval(pollStatus,    29000);},5000);"));
  client.println(F("  setTimeout(function(){setInterval(pollSysInfo,   31000);},10000);"));
  client.println(F("  setTimeout(function(){setInterval(pollThresholds,37000);},15000);"));
  client.println(F("}"));
  client.println(F("setTimeout(bootUp,2000);"));
  client.println(F("</script></body></html>"));
}

// Thin wrappers — called from Aging_Room.ino router
void serveSkitPage(EthernetClient &client) {
  extern float skitTempThreshold, skitHumidThreshold;
  serveRoomPage(client,
    "Skit Room", "/skit", "S",
    "#0072B2", "#E69F00",
    "Skit",
    skitTempThreshold, skitHumidThreshold,
    "skit");
}

void serveCameraPage(EthernetClient &client) {
  extern float camTempThreshold, camHumidThreshold;
  serveRoomPage(client,
    "Camera Room", "/camera", "C",
    "#CC79A7", "#56B4E9",
    "Camera",
    camTempThreshold, camHumidThreshold,
    "camera");
}

// ============================================================
// ADMIN PAGE — Hidden SD card file manager
// Route: GET /admin
// Protected by existing Basic Auth (no nav link — URL only)
// ============================================================
void serveAdminPage(EthernetClient &client) {
  extern unsigned long currentEpoch;

  client.println(F("HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: close\n"));
  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<title>Admin - SD Card Manager</title>"));
  client.println(F("<style>"));
  client.println(F("body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;padding:20px;margin:0;}"));
  client.println(F("h1{color:#e94560;margin-bottom:4px;}"));
  client.println(F(".subtitle{color:#888;font-size:13px;margin-bottom:24px;}"));
  client.println(F("table{width:100%;border-collapse:collapse;background:#16213e;border-radius:8px;overflow:hidden;}"));
  client.println(F("th{background:#0f3460;color:#e0e0e0;padding:10px 14px;text-align:left;font-size:13px;}"));
  client.println(F("td{padding:9px 14px;border-bottom:1px solid #0f3460;font-size:13px;vertical-align:middle;}"));
  client.println(F("tr:last-child td{border-bottom:none;}"));
  client.println(F("tr:hover td{background:#1a2a4a;}"));
  client.println(F(".btn{display:inline-block;padding:4px 10px;border-radius:4px;text-decoration:none;font-size:12px;font-weight:bold;cursor:pointer;border:none;}"));
  client.println(F(".dl{background:#0072B2;color:#fff;margin-right:4px;}"));
  client.println(F(".del{background:#e94560;color:#fff;}"));
  client.println(F(".del:hover{background:#c73652;}"));
  client.println(F(".sz{color:#aaa;font-size:12px;}"));
  client.println(F(".warn{background:#3a1a1a;border:1px solid #e94560;border-radius:6px;padding:12px 16px;margin-bottom:20px;font-size:13px;color:#e94560;}"));
  client.println(F(".back{display:inline-block;margin-bottom:18px;color:#56B4E9;font-size:13px;text-decoration:none;}"));
  client.println(F(".back:hover{text-decoration:underline;}"));
  client.println(F(".empty{color:#666;padding:16px;text-align:center;}"));
  client.println(F("</style></head><body>"));

  client.println(F("<a class='back' href='/'>&#8592; Back to Aging Room</a>"));
  client.println(F("<h1>&#9881; SD Card Admin</h1>"));
  client.print(F("<div class='subtitle'>"));
  if (currentEpoch > 1000000000UL) {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(currentEpoch, y, mo, d, h, mi, s, wd);
    char ts[20];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d", y, mo+1, d, h, mi, s);
    client.print(ts);
  } else {
    client.print(F("Time not synced"));
  }
  client.println(F(" &mdash; SD File Manager</div>"));

  client.println(F("<div class='warn'>&#9888; Deleting files is permanent and cannot be undone.</div>"));

  wdt_reset();

  // --- Build file table by walking SD root ---
  client.println(F("<table><tr><th>Filename</th><th>Size</th><th>Actions</th></tr>"));

  File root = SD.open("/");
  if (!root) {
    client.println(F("<tr><td colspan='3' class='empty'>Could not open SD card root.</td></tr>"));
  } else {
    bool anyFiles = false;
    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;
      if (entry.isDirectory()) { entry.close(); continue; }

      anyFiles = true;
      char fname[16];
      strncpy(fname, entry.name(), 15);
      fname[15] = '\0';
      unsigned long fsize = entry.size();
      entry.close();

      wdt_reset();

      client.print(F("<tr><td>"));
      client.print(fname);
      client.print(F("</td><td class='sz'>"));

      // Human-readable size
      if (fsize < 1024UL) {
        client.print(fsize); client.print(F(" B"));
      } else if (fsize < 1048576UL) {
        client.print(fsize / 1024UL); client.print(F(" KB"));
      } else {
        client.print(fsize / 1048576UL); client.print(F(" MB"));
      }

      client.print(F("</td><td>"));
      // Download button — reuses existing /archive endpoint
      client.print(F("<a class='btn dl' href='/archive?file="));
      client.print(fname);
      client.print(F("'>&#11015; Download</a>"));
      // Delete button
      client.print(F("<a class='btn del' href='/admin/delete?file="));
      client.print(fname);
      client.print(F("' onclick=\"return confirm('Delete "));
      client.print(fname);
      client.print(F("?')\">&#128465; Delete</a>"));
      client.println(F("</td></tr>"));
    }
    root.close();
    if (!anyFiles) {
      client.println(F("<tr><td colspan='3' class='empty'>No files found on SD card.</td></tr>"));
    }
  }

  client.println(F("</table>"));
  client.println(F("</body></html>"));
}

// ============================================================
// ADMIN DELETE handler
// Route: GET /admin/delete?file=FILENAME
// ============================================================
void handleAdminDelete(EthernetClient &client, const char *filename) {
  if (filename == nullptr || strlen(filename) == 0) {
    client.println(F("HTTP/1.1 400 Bad Request\nContent-Type: text/plain\nConnection: close\n\nMissing filename."));
    return;
  }

  // Safety: block deleting active live data files
  if (strcmp(filename, "temp.csv")  == 0 ||
      strcmp(filename, "humid.csv") == 0 ||
      strcmp(filename, "SK_T.csv")  == 0 ||
      strcmp(filename, "SK_H.csv")  == 0 ||
      strcmp(filename, "CA_T.csv")  == 0 ||
      strcmp(filename, "CA_H.csv")  == 0) {
    client.println(F("HTTP/1.1 403 Forbidden\nContent-Type: text/html\nConnection: close\n"));
    client.print(F("<h3>&#128683; Cannot delete active data file: "));
    client.print(filename);
    client.println(F("</h3><p>Use the /cleanup endpoint instead, or rename first.</p><a href='/admin'>Back</a>"));
    return;
  }

  if (SD.exists(filename)) {
    SD.remove(filename);
    client.println(F("HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: close\n"));
    client.print(F("<meta http-equiv='refresh' content='1;url=/admin'>"));
    client.print(F("<p>&#9989; Deleted: "));
    client.print(filename);
    client.println(F("</p><a href='/admin'>Back to Admin</a>"));
  } else {
    client.println(F("HTTP/1.1 404 Not Found\nContent-Type: text/html\nConnection: close\n"));
    client.print(F("<p>&#10060; File not found: "));
    client.print(filename);
    client.println(F("</p><a href='/admin'>Back to Admin</a>"));
  }
}
