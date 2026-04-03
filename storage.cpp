#include "storage.h"
#include "network.h"
#include "sensors.h"
#include "display.h"
#include <Ethernet.h>
#include <SD.h>
#include <avr/wdt.h>

extern LiquidCrystal_I2C lcd;

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

unsigned long lastCsvWrite = 0;

void initSDCard() {
  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("SD card initialization failed!");
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

  char tFile[13]; snprintf(tFile, sizeof(tFile), "%02d%02d%02d_T.csv", year % 100, month + 1, day);
  char hFile[13]; snprintf(hFile, sizeof(hFile), "%02d%02d%02d_H.csv", year % 100, month + 1, day);

  ensureDailyHeader(tFile);
  ensureDailyHeader(hFile);

  String dateStr = getDateString();
  String timeStr = getTimeString();

  extern float tA, tB, tC, tD, hA, hB, hC, hD;

  File tf = SD.open(tFile, FILE_WRITE);
  if (tf) {
    tf.print(dateStr + "," + timeStr + ",");
    tf.print(isnan(tA) ? "ERR" : String(tA, 1) + " C"); tf.print(",");
    tf.print(isnan(tB) ? "ERR" : String(tB, 1) + " C"); tf.print(",");
    tf.print(isnan(tC) ? "ERR" : String(tC, 1) + " C"); tf.print(",");
    tf.println(isnan(tD) ? "ERR" : String(tD, 1) + " C");
    tf.close();
  }

  File hf = SD.open(hFile, FILE_WRITE);
  if (hf) {
    hf.print(dateStr + "," + timeStr + ",");
    hf.print(isnan(hA) ? "ERR" : String(hA, 1) + " %"); hf.print(",");
    hf.print(isnan(hB) ? "ERR" : String(hB, 1) + " %"); hf.print(",");
    hf.print(isnan(hC) ? "ERR" : String(hC, 1) + " %"); hf.print(",");
    hf.println(isnan(hD) ? "ERR" : String(hD, 1) + " %");
    hf.close();
  }
}

void purgeOldLogs() {
  extern unsigned long currentEpoch;
  if (currentEpoch < 1000000000UL) return; 

  unsigned long purgeEpoch = currentEpoch - (180UL * 86400UL); 
  int y, mo, d, h, mi, s, wd;
  epochToDateTime(purgeEpoch, y, mo, d, h, mi, s, wd);
  
  char tFile[13]; snprintf(tFile, sizeof(tFile), "%02d%02d%02d_T.csv", y % 100, mo + 1, d);
  char hFile[13]; snprintf(hFile, sizeof(hFile), "%02d%02d%02d_H.csv", y % 100, mo + 1, d);
  
  if (SD.exists(tFile)) SD.remove(tFile);
  if (SD.exists(hFile)) SD.remove(hFile);
}

void serveFile(EthernetClient &client, const char *filename, const char *contentType) {
  if (strcmp(filename, "EVENTS.txt") == 0 && !SD.exists(filename)) {
    client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n\n");
    return;
  }

  if (strcmp(filename, "temp.csv") == 0 || strcmp(filename, "humid.csv") == 0) {
    bool isTemp = (strcmp(filename, "temp.csv") == 0);

    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: "); client.println(contentType);
    client.print("Content-Disposition: inline; filename=\""); client.print(filename); client.println("\"");
    client.println("Connection: close\n");
    client.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");

    extern unsigned long currentEpoch;
    for (int i = 6; i >= 0; i--) {
      unsigned long targetEpoch = currentEpoch - (i * 86400UL);
      int y, mo, d, h, mi, s, wd;
      epochToDateTime(targetEpoch, y, mo, d, h, mi, s, wd);

      char fn[13];
      if (isTemp) snprintf(fn, sizeof(fn), "%02d%02d%02d_T.csv", y % 100, mo + 1, d);
      else        snprintf(fn, sizeof(fn), "%02d%02d%02d_H.csv", y % 100, mo + 1, d);

      if (SD.exists(fn)) {
        File f = SD.open(fn, FILE_READ);
        if (f) {
          while(f.available()) { wdt_reset(); if (f.read() == '\n') break; } 
          while(f.available()) { client.write(f.read()); wdt_reset(); }
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
    while (file.available()) { client.write(file.read()); wdt_reset(); }
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
  extern float tempThreshold;
  client.println("HTTP/1.1 200 OK\nContent-Type: text/plain\nConnection: close\n");
  const char* labels[] = {"A", "B", "C", "D"};
  float temps[] = {tA, tB, tC, tD};
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
  client.print("SD:"); client.print(SD.begin(SD_CHIP_SELECT) ? "OK" : "FAIL"); client.print(",");

  if (lastCsvWrite == 0) { client.print("LASTWRITE:Never"); } 
  else {
    extern unsigned long currentEpoch;
    unsigned long secsAgo = (millis() - lastCsvWrite) / 1000;
    unsigned long writeEpoch = currentEpoch - secsAgo;
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(writeEpoch, y, mo, d, h, mi, s, wd);
    const char* ampm = (h >= 12) ? "PM" : "AM";
    int displayHour = h % 12; if (displayHour == 0) displayHour = 12;
    char buf[24]; snprintf(buf, sizeof(buf), "%02d-%02d-%04d %d:%02d:%02d %s", mo+1, d, y, displayHour, mi, s, ampm);
    client.print("LASTWRITE:"); client.print(buf);
  }
  client.print(",");

  if (lastNtpEpoch == 0) { client.print("NTPSYNC:Never"); } 
  else {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(lastNtpEpoch, y, mo, d, h, mi, s, wd);
    const char* ampm = (h >= 12) ? "PM" : "AM";
    int displayHour = h % 12; if (displayHour == 0) displayHour = 12;
    char buf[24]; snprintf(buf, sizeof(buf), "%02d-%02d-%04d %d:%02d:%02d %s", mo+1, d, y, displayHour, mi, s, ampm);
    client.print("NTPSYNC:"); client.print(buf);
  }
  client.println();
}

void serveRootPage(EthernetClient &client) {
  String lastUpdate = getDateString() + " " + getTimeString();
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
  
  wdt_reset(); // WATCHDOG CHECKPOINT 1

  client.println(F(".sensor-dot.s-a{background:#0072B2;} .sensor-label.s-a{color:#0072B2;}")); 
  client.println(F(".sensor-dot.s-b{background:#E69F00;} .sensor-label.s-b{color:#E69F00;}")); 
  client.println(F(".sensor-dot.s-c{background:#CC79A7;} .sensor-label.s-c{color:#CC79A7;}")); 
  client.println(F(".sensor-dot.s-d{background:#56B4E9;} .sensor-label.s-d{color:#56B4E9;}")); 
  client.println(F("</style>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns/dist/chartjs-adapter-date-fns.bundle.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/hammerjs@2.0.8/hammer.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@1.2.1/dist/chartjs-plugin-zoom.min.js'></script>"));
  client.println(F("</head><body>"));

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
    wdt_reset(); // WATCHDOG CHECKPOINT (Loop)
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
      float tC = initTemps[i]; float tF = tC * 9.0 / 5.0 + 32.0;
      client.print(F("</span><span class='sensor-temp'>")); client.print(tC, 1);
      client.print(F("°C (")); client.print(tF, 1); client.print(F("°F)"));
      if (isLow) client.print(F(" ↓ LOW")); else if (isHigh) client.print(F(" ↑ HIGH")); else client.print(F(" ✓"));
    }
    client.print(F("</span></span>  "));
  }
  client.println(F("</div>"));

  client.println(F("<div>"));
  client.println(F("<div class='tab active' onclick=\"showTab('temp', event)\">Temperature</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('humid', event)\">Humidity</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('archive', event)\">Archive Data</div>"));
  client.println(F("</div>"));

  wdt_reset(); // WATCHDOG CHECKPOINT 2

  client.println(F("<div id='temp' class='tab-content active'>"));
  client.println(F("<label>Range: <select id='tempRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(tempChart, \"temp\")'>Export PNG</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(tempChart&&tempChart.resetZoom)tempChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><br><div class='chart-scroll-wrapper'><canvas id='tempChart'></canvas></div></div>"));

  client.println(F("<div id='humid' class='tab-content'>"));
  client.println(F("<label>Range: <select id='humidRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
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

  wdt_reset(); // WATCHDOG CHECKPOINT 3

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
  client.println(F("let tempChart, humidChart;"));
  client.print(F("let threshold = ")); client.print(tempThreshold, 1); client.println(F("; const margin = 5.0;"));

  wdt_reset(); // WATCHDOG CHECKPOINT 4

  // --- NEW THREE STRIKES LOGIC ---
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
  // --------------------------------

  client.println(F("function showTab(id, evt){"));
  client.println(F("  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));"));
  client.println(F("  document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));"));
  client.println(F("  document.getElementById(id).classList.add('active'); evt.target.classList.add('active');"));
  client.println(F("}"));

  client.println(F("function downloadChart(chart, label){"));
  client.println(F("  if(!chart) return; const link = document.createElement('a');"));
  client.println(F("  link.download = label + '_chart.png'; link.href = chart.toBase64Image(); link.click();"));
  client.println(F("}"));

  wdt_reset(); // WATCHDOG CHECKPOINT 5

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

  wdt_reset(); // WATCHDOG CHECKPOINT 6

  client.println(F("async function safeEject() { if(confirm('STOP LOGGING AND UNMOUNT SD CARD?\\nYou MUST physically unplug and reboot the Arduino to resume logging.')) { try { await fetch('/eject'); document.body.innerHTML = '<div style=\"text-align:center;margin-top:100px;\"><h1 style=\"color:#e74c3c;\">System Halted Safely</h1><p>The SD card has been unmounted.</p><p>You may now safely unplug the power.</p></div>'; } catch(e) { handleDisconnect(); } } }"));

  client.println(F("const sensorMap = { 'A':{idClass:'s-a', color:'#0072B2'}, 'B':{idClass:'s-b', color:'#E69F00'}, 'C':{idClass:'s-c', color:'#CC79A7'}, 'D':{idClass:'s-d', color:'#56B4E9'} };"));

  client.println(F("function updateStatusBar(records) {"));
  client.println(F("  records.forEach(rec => {"));
  client.println(F("    const colonIdx = rec.indexOf(':'); if (colonIdx === -1) return;"));
  client.println(F("    const label = rec.substring(0, colonIdx).trim(); const parts = rec.substring(colonIdx + 1).trim().split('|');"));
  client.println(F("    const tempC = parts[0], state = parts[1];"));
  client.println(F("    const isErr = state === 'ERR', isOk = state === 'OK', isLow = state === 'LOW', isHigh = state === 'HIGH';"));
  client.println(F("    const info = sensorMap[label]; if (!info) return;"));
  client.println(F("    const el = document.getElementById('statusSensor' + label);"));
  client.println(F("    if (el) {"));
  client.println(F("      el.querySelector('.sensor-dot').className = 'sensor-dot ' + info.idClass + (isErr ? ' dot-err' : !isOk ? ' dot-warn' : '');"));
  client.println(F("      const lbl = el.querySelector('.sensor-label'); lbl.className = 'sensor-label ' + info.idClass + (isErr ? ' sensor-err' : !isOk ? ' sensor-warn' : '');"));
  client.println(F("      if (isErr) { lbl.textContent = label + ' ERR'; el.querySelector('.sensor-temp').textContent = ''; }"));
  client.println(F("      else { lbl.textContent = label; const cVal = parseFloat(tempC), fVal = (cVal * 9 / 5 + 32).toFixed(1); const suffix = isLow ? ' \u2193 LOW' : isHigh ? ' \u2191 HIGH' : ' \u2713'; el.querySelector('.sensor-temp').textContent = cVal.toFixed(1) + '\u00b0C (' + fVal + '\u00b0F)' + suffix; }"));
  client.println(F("    }"));
  client.println(F("    if (tempChart) { const ds = tempChart.data.datasets.find(d => d.label === 'Sensor ' + label); if (ds) { ds.borderColor = isErr ? '#e74c3c' : info.color; ds.backgroundColor = ds.borderColor; ds.pointBackgroundColor = ds.borderColor; } }"));
  client.println(F("  }); if (tempChart) tempChart.update();"));
  client.println(F("}"));

  wdt_reset(); // WATCHDOG CHECKPOINT 7

  client.println(F("async function safeFetch(url) {"));
  client.println(F("  if (isOffline && !url.includes('/status')) return null;")); 
  client.println(F("  try { let res = await fetch(url); if (!res.ok && res.status !== 404 && res.status !== 401) throw new Error('Bad Network'); failedPings = 0; return res; }"));
  client.println(F("  catch (e) { handleDisconnect(); return null; }"));
  client.println(F("}"));

  client.println(F("async function fetchData(filename, rangeDays) {"));
  client.println(F("  let res = await safeFetch('/' + filename + '?t=' + new Date().getTime());"));
  client.println(F("  if (!res || !res.ok) return {labels:[], sensorsA:[], sensorsB:[], sensorsC:[], sensorsD:[]};"));
  client.println(F("  let text = await res.text(); let lines = text.trim().split('\\n').slice(1);"));
  client.println(F("  let limit = new Date().getTime() - rangeDays * 86400000;"));
  client.println(F("  let labels=[], sensorsA=[], sensorsB=[], sensorsC=[], sensorsD=[];"));
  client.println(F("  let downsampleRate = rangeDays <= 1 ? 1 : rangeDays <= 3 ? 6 : 12;"));
  client.println(F("  lines.forEach((line, idx) => {"));
  client.println(F("    if (idx % downsampleRate !== 0) return;"));
  client.println(F("    let [date, time, a, b, c, d] = line.split(','); if (!date || !time) return;"));
  client.println(F("    let dtStr = date.split('-').join('/') + ' ' + time; let dt = new Date(dtStr);"));
  client.println(F("    if(dt.getTime() >= limit){ labels.push(dt.getTime()); sensorsA.push(parseFloat(a) || null); sensorsB.push(parseFloat(b) || null); sensorsC.push(parseFloat(c) || null); sensorsD.push(parseFloat(d) || null); }"));
  client.println(F("  });"));
  client.println(F("  return {labels, sensorsA, sensorsB, sensorsC, sensorsD};"));
  client.println(F("}"));

  client.println(F("async function pollThreshold() { if(isOffline) return; try { let res = await safeFetch('/threshold?t=' + new Date().getTime()); if(res){ let val = parseFloat(await res.text()); if (!isNaN(val) && val !== threshold) { threshold = val; updateThresholdLines(); } } } catch(e) { handleDisconnect(); } }"));
  client.println(F("async function pollStatus() { if(isOffline) return; try { let res = await safeFetch('/status?t=' + new Date().getTime()); if(res){ let text = await res.text(); updateStatusBar(text.trim().split(',')); } } catch(e) { handleDisconnect(); } }"));

  client.println(F("function updateThresholdLines() {"));
  client.println(F("  if (tempChart) { let len = tempChart.data.labels.length;"));
  client.println(F("    tempChart.data.datasets[4].data = Array(len).fill(threshold);"));
  client.println(F("    tempChart.data.datasets[5].data = Array(len).fill(threshold + margin);"));
  client.println(F("    tempChart.data.datasets[6].data = Array(len).fill(threshold - margin);"));
  client.println(F("    tempChart.update();"));
  client.println(F("  }"));
  client.println(F("}"));

  wdt_reset(); // WATCHDOG CHECKPOINT 8

  client.println(F("async function updateCharts(){"));
  client.println(F("  if (isOffline) return;"));
  client.println(F("  let rangeT = parseInt(document.getElementById('tempRange').value); let rangeH = parseInt(document.getElementById('humidRange').value);"));
  client.println(F("  let tempData = await fetchData('temp.csv', rangeT); let humidData = await fetchData('humid.csv', rangeH);"));

  // ==========================================
  // ISOLATED CHART RENDERING - SHATTERED STRINGS FOR COMPILER SAFETY
  // ==========================================
  client.println(F("  if (tempData.labels.length > 0) {"));
  client.println(F("    if(tempChart) tempChart.destroy();"));
  client.println(F("    tempChart = new Chart(document.getElementById('tempChart'), {"));
  client.println(F("      type: 'line', data: { labels: tempData.labels, datasets: ["));
  client.println(F("        {label: 'Sensor A', data: tempData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: tempData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: tempData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: tempData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Threshold', data: Array(tempData.labels.length).fill(threshold), borderColor: 'black', borderDash: [5,5], pointRadius: 0},"));
  client.println(F("        {label: 'High Threshold', data: Array(tempData.labels.length).fill(threshold + margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0},"));
  client.println(F("        {label: 'Low Threshold', data: Array(tempData.labels.length).fill(threshold - margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0}"));
  client.println(F("      ]},"));
  client.println(F("      options: { responsive: true, maintainAspectRatio: false, layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: { x: { type: 'time', time: { tooltipFormat: 'MM/dd/yyyy h:mm a', displayFormats: { hour: 'h:mm a', minute: 'h:mm a', day: 'MMM d' } }, ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } } },"));
  client.println(F("      y: { title: { display: true, text: 'Celsius (°C)', font: { size: 13 } }, ticks: { stepSize: 1.0 } } },"));
  client.println(F("      interaction: { mode: 'index', intersect: false }, plugins: { tooltip: { mode: 'index', intersect: false },"));
  client.println(F("      legend: { labels: { boxWidth: 24, padding: 16, font: { size: 13 } }, onClick: function(e, legendItem, legend) { const index = legendItem.datasetIndex; const ci = legend.chart; if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); } } },"));
  client.println(F("      zoom: typeof ChartZoom !== 'undefined' ? { pan: { enabled: true, mode: 'x' }, zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' } } : {} } }"));
  client.println(F("    });"));
  client.println(F("  }"));

  wdt_reset(); // WATCHDOG CHECKPOINT 9

  client.println(F("  if (humidData.labels.length > 0) {"));
  client.println(F("    if(humidChart) humidChart.destroy();"));
  client.println(F("    humidChart = new Chart(document.getElementById('humidChart'), {"));
  client.println(F("      type: 'line', data: { labels: humidData.labels, datasets: ["));
  client.println(F("        {label: 'Sensor A', data: humidData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: humidData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: humidData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: humidData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4}"));
  client.println(F("      ]},"));
  client.println(F("      options: { responsive: true, maintainAspectRatio: false, layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: { x: { type: 'time', time: { tooltipFormat: 'MM/dd/yyyy h:mm a', displayFormats: { hour: 'h:mm a', minute: 'h:mm a', day: 'MMM d' } }, ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } } },"));
  client.println(F("      y: { title: { display: true, text: 'Humidity (%)', font: { size: 13 } }, ticks: { stepSize: 1.0 } } },"));
  client.println(F("      interaction: { mode: 'index', intersect: false }, plugins: { tooltip: { mode: 'index', intersect: false },"));
  client.println(F("      legend: { labels: { boxWidth: 24, padding: 16, font: { size: 13 } }, onClick: function(e, legendItem, legend) { const index = legendItem.datasetIndex; const ci = legend.chart; if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); } } },"));
  client.println(F("      zoom: typeof ChartZoom !== 'undefined' ? { pan: { enabled: true, mode: 'x' }, zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' } } : {} } }"));
  client.println(F("    });"));
  client.println(F("  }"));

  client.println(F("  setTimeout(pollStatus, 500); updateLastUpdate();"));
  client.println(F("}"));

  wdt_reset(); // WATCHDOG CHECKPOINT 10

  // Split massive SysInfo string so compiler handles it perfectly
  client.println(F("async function pollSysInfo() { if(isOffline) return; try { const res = await safeFetch('/sysinfo?t=' + new Date().getTime()); if(!res) return; const text = await res.text(); const pairs = {};"));
  client.println(F("  text.trim().split(',').forEach(p => { const i = p.indexOf(':'); if (i !== -1) pairs[p.substring(0,i).trim()] = p.substring(i+1).trim(); });"));
  client.println(F("  const ram = parseInt(pairs['RAM'] || 0); const ramColor = ram > 2000 ? '#2ecc71' : ram > 1000 ? '#f39c12' : '#e74c3c';"));
  client.println(F("  const ramEl = document.getElementById('sysRam'); if (ramEl) ramEl.innerHTML = 'RAM: <span style=\"color:' + ramColor + '; font-weight:bold;\">' + (ram/1024).toFixed(1) + ' KB</span>';"));
  client.println(F("  const upEl = document.getElementById('sysUptime'); if (upEl) upEl.innerHTML = '&#9201; Uptime: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['UPTIME'] || '--') + '</span>';"));
  client.println(F("  const sdEl = document.getElementById('sysSd'); if (sdEl) { sdEl.textContent = 'SD: ' + (pairs['SD'] || '--'); sdEl.style.color = pairs['SD']==='OK' ? '#27ae60' : '#e74c3c'; }"));
  client.println(F("  const wrEl = document.getElementById('sysWrite'); if (wrEl) wrEl.innerHTML = '&#128221; Last Write: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['LASTWRITE'] || '--') + '</span>';"));
  client.println(F("  const ntpEl = document.getElementById('sysNtp'); if (ntpEl) ntpEl.innerHTML = '&#128336; NTP Sync: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['NTPSYNC'] || '--') + '</span>';"));
  client.println(F("} catch(e) { handleDisconnect(); } }"));
  
  client.println(F("function updateLastUpdate() { document.getElementById('lastUpdate').textContent = new Date().toLocaleString(); }"));

  client.println(F("async function clearEvents() { if (confirm('Clear alert history?')) { try { await fetch('/clear-events'); document.getElementById('eventList').innerHTML = '<li>No recent alerts.</li>'; } catch(e) { handleDisconnect(); } } }"));
  
  client.println(F("async function pollEvents() { if(isOffline) return; try { let res = await safeFetch('/events?t=' + new Date().getTime()); if (res && res.ok) { let text = await res.text(); let lines = text.trim().split('\\n').filter(l => l.trim().length > 0); let last5 = lines.slice(-5).reverse(); let html = ''; if (last5.length === 0) { html = '<li>No recent alerts.</li>'; } else { last5.forEach(l => html += '<li>' + l + '</li>'); } document.getElementById('eventList').innerHTML = html; } else if (res) { document.getElementById('eventList').innerHTML = '<li>No alert log found on SD card yet.</li>'; } } catch(e) { handleDisconnect(); } }"));

  // --- BOOT SEQUENCE & PRIME NUMBER TIMERS ---
  client.println(F("async function bootUp() {"));
  client.println(F("  await updateCharts();")); 
  client.println(F("  await pollStatus();"));
  client.println(F("  await pollSysInfo();"));
  client.println(F("  await pollThreshold();"));
  client.println(F("  await pollEvents();"));
  
  client.println(F("  setInterval(updateCharts, 307000);"));  // 5 mins and 7 seconds
  client.println(F("  setInterval(pollStatus, 29000);"));     // 29 seconds
  client.println(F("  setInterval(pollSysInfo, 31000);"));    // 31 seconds
  client.println(F("  setInterval(pollThreshold, 37000);"));  // 37 seconds
  client.println(F("  setInterval(pollEvents, 53000);"));     // 53 seconds
  client.println(F("}"));

  client.println(F("setTimeout(bootUp, 2000);")); // Let the Arduino catch its breath for 2 seconds!
  client.println(F("</script></body></html>"));
}