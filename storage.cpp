#include "storage.h"
#include "network.h"
#include "sensors.h"
#include "display.h"

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
    }
  } else {
    Serial.println("SD card initialized.");
  }
}

void createCsvHeaderIfNeeded() {
  // Legacy function kept to prevent errors, files created dynamically now
}

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

  // New YYMMDD Format for 6-month retention
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
    tf.print(isnan(tA) ? "ERR" : String(tA, 1) + " C");
    tf.print(",");
    tf.print(isnan(tB) ? "ERR" : String(tB, 1) + " C");
    tf.print(",");
    tf.print(isnan(tC) ? "ERR" : String(tC, 1) + " C");
    tf.print(",");
    tf.println(isnan(tD) ? "ERR" : String(tD, 1) + " C");
    tf.close();
  }

  File hf = SD.open(hFile, FILE_WRITE);
  if (hf) {
    hf.print(dateStr + "," + timeStr + ",");
    hf.print(isnan(hA) ? "ERR" : String(hA, 1) + " %");
    hf.print(",");
    hf.print(isnan(hB) ? "ERR" : String(hB, 1) + " %");
    hf.print(",");
    hf.print(isnan(hC) ? "ERR" : String(hC, 1) + " %");
    hf.print(",");
    hf.println(isnan(hD) ? "ERR" : String(hD, 1) + " %");
    hf.close();
  }
}

void purgeOldLogs() {
  extern unsigned long currentEpoch;
  if (currentEpoch < 1000000000UL) return; 

  // Calculate exactly 180 days ago
  unsigned long purgeEpoch = currentEpoch - (180UL * 86400UL); 
  int y, mo, d, h, mi, s, wd;
  epochToDateTime(purgeEpoch, y, mo, d, h, mi, s, wd);
  
  char tFile[13]; snprintf(tFile, sizeof(tFile), "%02d%02d%02d_T.csv", y % 100, mo + 1, d);
  char hFile[13]; snprintf(hFile, sizeof(hFile), "%02d%02d%02d_H.csv", y % 100, mo + 1, d);
  
  if (SD.exists(tFile)) SD.remove(tFile);
  if (SD.exists(hFile)) SD.remove(hFile);
}

void serveFile(EthernetClient &client, const char *filename, const char *contentType) {
  // Live Graph Dynamic 7-Day Stitcher
  if (strcmp(filename, "temp.csv") == 0 || strcmp(filename, "humid.csv") == 0) {
    bool isTemp = (strcmp(filename, "temp.csv") == 0);

    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.print("Content-Disposition: inline; filename=\"");
    client.print(filename);
    client.println("\"");
    client.println("Connection: close\n");

    client.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");

    extern unsigned long currentEpoch;
    
    // Stream chronologically: 6 days ago up to today
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
          while(f.available()) { if (f.read() == '\n') break; } // Skip header
          while(f.available()) { client.write(f.read()); }
          f.close();
        }
      }
    }
    return;
  }

  // Fallback for files like favicon (if added later)
  if (SD.exists(filename)) {
    File file = SD.open(filename, FILE_READ);
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.println("Connection: close\n");
    while (file.available()) { client.write(file.read()); }
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
    client.print(labels[i]);
    client.print(":");
    if (isnan(temps[i])) {
      client.print("ERR|ERR");
    } else {
      client.print(temps[i], 1);
      client.print("|");
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
  unsigned long secs  = ms / 1000;
  unsigned long mins  = secs / 60;
  unsigned long hours = mins / 60;
  unsigned long days  = hours / 24;
  client.print("UPTIME:"); client.print(days); client.print("d ");
  client.print(hours % 24); client.print("h "); client.print(mins % 60); client.print("m,");

  client.print("SD:"); client.print(SD.begin(SD_CHIP_SELECT) ? "OK" : "FAIL"); client.print(",");

  if (lastCsvWrite == 0) {
    client.print("LASTWRITE:Never");
  } else {
    extern unsigned long currentEpoch;
    unsigned long secsAgo = (millis() - lastCsvWrite) / 1000;
    unsigned long writeEpoch = currentEpoch - secsAgo;
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(writeEpoch, y, mo, d, h, mi, s, wd);
    const char* ampm = (h >= 12) ? "PM" : "AM";
    int displayHour = h % 12;
    if (displayHour == 0) displayHour = 12;
    char buf[24];
    snprintf(buf, sizeof(buf), "%02d-%02d-%04d %d:%02d:%02d %s", mo+1, d, y, displayHour, mi, s, ampm);
    client.print("LASTWRITE:"); client.print(buf);
  }
  client.print(",");

  if (lastNtpEpoch == 0) {
    client.print("NTPSYNC:Never");
  } else {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(lastNtpEpoch, y, mo, d, h, mi, s, wd);
    const char* ampm = (h >= 12) ? "PM" : "AM";
    int displayHour = h % 12;
    if (displayHour == 0) displayHour = 12;
    char buf[24];
    snprintf(buf, sizeof(buf), "%02d-%02d-%04d %d:%02d:%02d %s", mo+1, d, y, displayHour, mi, s, ampm);
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
  client.println(F(".tab-content{display:none;}"));
  client.println(F(".tab-content.active{display:block;}"));
  client.println(F("button{margin-left:10px;padding:6px 12px;cursor:pointer;}"));
  client.println(F(".chart-scroll-wrapper{width:100%;background:#fff;border-radius:6px;padding:10px;box-sizing:border-box;}"));
  client.println(F(".chart-scroll-wrapper canvas{height:500px !important;display:block;}"));
  client.println(F("#statusBar{display:flex;gap:20px;align-items:center;padding:12px 16px;margin-bottom:12px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:15px;flex-wrap:wrap;}"));
  client.println(F(".sensor-dot{display:inline-block;width:14px;height:14px;border-radius:50%;margin-right:6px;vertical-align:middle;}"));
  client.println(F("@keyframes errorBlink { 0% { opacity: 1; } 50% { opacity: 0; } 100% { opacity: 1; } }"));
  client.println(F(".dot-warn{background:#f39c12;} .dot-err{background:#e74c3c; animation: errorBlink 1s infinite;}"));
  client.println(F(".sensor-label{font-weight:bold;} .sensor-warn{color:#f39c12;} .sensor-err{color:#e74c3c; animation: errorBlink 1s infinite;}"));
  client.println(F(".sensor-temp{color:#555;font-size:14px;margin-left:4px;}"));
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
  client.print(F("<p>Last update: <span id='lastUpdate'>"));
  client.print(lastUpdate);
  client.println(F("</span></p>"));

  client.println(F("<div id='statusBar'><strong>Sensors:</strong>"));
  const char* sensorLabels[] = {"A", "B", "C", "D"};
  const char* sensorIdClasses[] = {"s-a", "s-b", "s-c", "s-d"};
  float initTemps[] = {tA, tB, tC, tD};
  for (int i = 0; i < 4; i++) {
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

  // --- UPGRADED: 6-MONTH DATE RANGE ARCHIVE TAB ---
  client.println(F("<div id='archive' class='tab-content'>"));
  client.println(F("  <div style='background:#fff; padding:20px; border-radius:6px; border:1px solid #ddd; margin-top:10px;'>"));
  client.println(F("    <h3 style='margin-top:0;'>Download Historical Data Range (Up to 6 Months)</h3>"));
  client.println(F("    <p>Select a date range to generate a combined CSV report. Data is strictly retained for 180 days.</p>"));
  client.println(F("    <label><b>From:</b> <input type='date' id='startDate' style='padding:6px; border:1px solid #ccc; border-radius:4px;'></label>"));
  client.println(F("    <label style='margin-left: 15px;'><b>To:</b> <input type='date' id='endDate' style='padding:6px; border:1px solid #ccc; border-radius:4px;'></label><br><br>"));
  client.println(F("    <label><b>Data Type:</b> <select id='dataType' style='padding:6px; border:1px solid #ccc; border-radius:4px;'><option value='T'>Temperature</option><option value='H'>Humidity</option></select></label><br><br>"));
  client.println(F("    <button id='dlButton' onclick='downloadDateRange()' style='padding:10px 16px; font-weight:bold; background:#2980b9; color:white; border:none; border-radius:4px; cursor:pointer;'>Download Combined CSV</button>"));
  client.println(F("    <span id='dlProgress' style='margin-left: 10px; font-weight: bold; color: #27ae60;'></span>"));
  client.println(F("  </div>"));
  client.println(F("</div>"));

  // --- UPGRADED: ADDED SAFE EJECT TO SYSTEM STATUS ---
  client.println(F("<div id='sysPanel' style='margin-top:16px;padding:12px 16px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:14px;'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:8px;font-size:15px;display:flex;justify-content:space-between;'><span>System Status</span>"));
  client.println(F("<button onclick='safeEject()' style='background:#e74c3c; color:white; border:none; padding:4px 8px; border-radius:4px; font-weight:bold; cursor:pointer;'>Prepare SD for Removal / Halt</button></div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;'>"));
  client.println(F("<span id='sysRam'>RAM: --</span> <span id='sysUptime'>&#9201; Uptime: --</span> <span id='sysSd'>&#128190; SD: --</span>"));
  client.println(F("</div><div style='display:flex;flex-wrap:wrap;gap:20px;margin-top:6px;'>"));
  client.println(F("<span id='sysWrite'>&#128221; Last Write: --</span> <span id='sysNtp'>&#128336; NTP Sync: --</span>"));
  client.println(F("</div></div>"));

  client.println(F("<script>"));
  client.println(F("let tempChart, humidChart;"));
  client.print(F("let threshold = ")); client.print(tempThreshold, 1); client.println(F("; const margin = 5.0;"));

  client.println(F("function showTab(id, evt){"));
  client.println(F("  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));"));
  client.println(F("  document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));"));
  client.println(F("  document.getElementById(id).classList.add('active'); evt.target.classList.add('active');"));
  client.println(F("}"));

  client.println(F("function downloadChart(chart, label){"));
  client.println(F("  if(!chart) return; const link = document.createElement('a');"));
  client.println(F("  link.download = label + '_chart.png'; link.href = chart.toBase64Image(); link.click();"));
  client.println(F("}"));

  // --- UPGRADED: NEW MULTI-DAY STITCHING JS ---
  client.println(F("async function downloadDateRange() {"));
  client.println(F("  const startDate = document.getElementById('startDate').value;"));
  client.println(F("  const endDate = document.getElementById('endDate').value;"));
  client.println(F("  const dataType = document.getElementById('dataType').value;"));
  client.println(F("  const btn = document.getElementById('dlButton');"));
  client.println(F("  const progress = document.getElementById('dlProgress');"));
  client.println(F("  if (!startDate || !endDate) { alert('Please select both a Start and End date.'); return; }"));
  client.println(F("  const d1 = new Date(startDate + 'T12:00:00');"));
  client.println(F("  const d2 = new Date(endDate + 'T12:00:00');"));
  client.println(F("  if (d1 > d2) { alert('Start date must be BEFORE the End date.'); return; }"));
  client.println(F("  const diffDays = Math.round((d2 - d1) / 86400000);"));
  client.println(F("  if (diffDays > 180) { alert('You can only download up to 6 months (180 days) of data at a time.'); return; }"));
  
  client.println(F("  btn.disabled = true; btn.style.backgroundColor = '#95a5a6';"));
  client.println(F("  let combinedCsv = 'Date,Time,Sensor A,Sensor B,Sensor C,Sensor D\\n';"));
  client.println(F("  let filesFound = 0;"));
  
  client.println(F("  for (let i = 0; i <= diffDays; i++) {"));
  client.println(F("    let curr = new Date(d1.getTime() + (i * 86400000));"));
  client.println(F("    let y = curr.getFullYear().toString().slice(-2);"));
  client.println(F("    let m = (curr.getMonth() + 1).toString().padStart(2, '0');"));
  client.println(F("    let d = curr.getDate().toString().padStart(2, '0');"));
  client.println(F("    let filename = y + m + d + '_' + dataType + '.csv';"));
  client.println(F("    progress.innerText = 'Fetching day ' + (i + 1) + ' of ' + (diffDays + 1) + '...';"));
  
  client.println(F("    try {"));
  client.println(F("      let res = await fetch('/archive?file=' + filename);"));
  client.println(F("      if (res.ok) {"));
  client.println(F("        let text = await res.text();"));
  client.println(F("        let lines = text.trim().split('\\n');"));
  client.println(F("        if (lines.length > 1) { combinedCsv += lines.slice(1).join('\\n') + '\\n'; filesFound++; }"));
  client.println(F("      }"));
  client.println(F("    } catch(e) { console.log('Skipped missing file: ' + filename); }"));
  client.println(F("    await new Promise(resolve => setTimeout(resolve, 100));")); // 100ms pause to protect Arduino RAM
  client.println(F("  }"));
  
  client.println(F("  btn.disabled = false; btn.style.backgroundColor = '#2980b9';"));
  client.println(F("  progress.innerText = 'Done!'; setTimeout(() => progress.innerText = '', 3000);"));
  client.println(F("  if (filesFound === 0) { alert('No data found for this range.'); return; }"));
  client.println(F("  const blob = new Blob([combinedCsv], { type: 'text/csv' });"));
  client.println(F("  const url = window.URL.createObjectURL(blob);"));
  client.println(F("  const a = document.createElement('a'); a.setAttribute('href', url);"));
  client.println(F("  a.setAttribute('download', 'AgingRoom_' + dataType + '_' + startDate + '_to_' + endDate + '.csv');"));
  client.println(F("  a.click(); window.URL.revokeObjectURL(url);"));
  client.println(F("}"));

  // --- UPGRADED: SAFE EJECT JS ---
  client.println(F("async function safeEject() { if(confirm('STOP LOGGING AND UNMOUNT SD CARD?\\nYou MUST physically unplug and reboot the Arduino to resume logging.')) { try { await fetch('/eject'); document.body.innerHTML = '<div style=\"text-align:center;margin-top:100px;\"><h1 style=\"color:#e74c3c;\">System Halted Safely</h1><p>The SD card has been unmounted.</p><p>You may now safely unplug the power.</p></div>'; } catch(e) { alert('Command sent.'); } } }"));


  client.println(F("const sensorMap = { 'A':{idClass:'s-a', color:'#0072B2'}, 'B':{idClass:'s-b', color:'#E69F00'}, 'C':{idClass:'s-c', color:'#CC79A7'}, 'D':{idClass:'s-d', color:'#56B4E9'} };"));

  client.println(F("function updateStatusBar(records) {"));
  client.println(F("  records.forEach(rec => {"));
  client.println(F("    const colonIdx = rec.indexOf(':'); if (colonIdx === -1) return;"));
  client.println(F("    const label = rec.substring(0, colonIdx).trim();"));
  client.println(F("    const parts = rec.substring(colonIdx + 1).trim().split('|');"));
  client.println(F("    const tempC = parts[0], state = parts[1];"));
  client.println(F("    const isErr = state === 'ERR', isOk = state === 'OK', isLow = state === 'LOW', isHigh = state === 'HIGH';"));
  client.println(F("    const info = sensorMap[label]; if (!info) return;"));
  client.println(F("    const el = document.getElementById('statusSensor' + label);"));
  client.println(F("    if (el) {"));
  client.println(F("      el.querySelector('.sensor-dot').className = 'sensor-dot ' + info.idClass + (isErr ? ' dot-err' : !isOk ? ' dot-warn' : '');"));
  client.println(F("      const lbl = el.querySelector('.sensor-label');"));
  client.println(F("      lbl.className = 'sensor-label ' + info.idClass + (isErr ? ' sensor-err' : !isOk ? ' sensor-warn' : '');"));
  client.println(F("      if (isErr) { lbl.textContent = label + ' ERR'; el.querySelector('.sensor-temp').textContent = ''; }"));
  client.println(F("      else {"));
  client.println(F("        lbl.textContent = label;"));
  client.println(F("        const cVal = parseFloat(tempC), fVal = (cVal * 9 / 5 + 32).toFixed(1);"));
  client.println(F("        const suffix = isLow ? ' \u2193 LOW' : isHigh ? ' \u2191 HIGH' : ' \u2713';"));
  client.println(F("        el.querySelector('.sensor-temp').textContent = cVal.toFixed(1) + '\u00b0C (' + fVal + '\u00b0F)' + suffix;"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("    if (tempChart) {"));
  client.println(F("      const ds = tempChart.data.datasets.find(d => d.label === 'Sensor ' + label);"));
  client.println(F("      if (ds) { ds.borderColor = isErr ? '#e74c3c' : info.color; ds.backgroundColor = ds.borderColor; ds.pointBackgroundColor = ds.borderColor; }"));
  client.println(F("    }"));
  client.println(F("  }); if (tempChart) tempChart.update();"));
  client.println(F("}"));

  client.println(F("async function fetchData(filename, rangeDays) {"));
  client.println(F("  let res = await fetch('/' + filename + '?t=' + new Date().getTime());"));
  client.println(F("  let text = await res.text();"));
  client.println(F("  let lines = text.trim().split('\\n').slice(1);"));
  client.println(F("  let limit = new Date().getTime() - rangeDays * 86400000;"));
  client.println(F("  let labels=[], sensorsA=[], sensorsB=[], sensorsC=[], sensorsD=[];"));
  client.println(F("  let downsampleRate = rangeDays <= 1 ? 1 : rangeDays <= 3 ? 6 : 12;"));
  client.println(F("  lines.forEach((line, idx) => {"));
  client.println(F("    if (idx % downsampleRate !== 0) return;"));
  client.println(F("    let [date, time, a, b, c, d] = line.split(',');"));
  client.println(F("    if (!date || !time) return;"));
  client.println(F("    let dtStr = date.split('-').join('/') + ' ' + time; let dt = new Date(dtStr);"));
  client.println(F("    if(dt.getTime() >= limit){"));
  client.println(F("      labels.push(dt.getTime()); sensorsA.push(parseFloat(a) || null); sensorsB.push(parseFloat(b) || null); sensorsC.push(parseFloat(c) || null); sensorsD.push(parseFloat(d) || null);"));
  client.println(F("    }"));
  client.println(F("  });"));
  client.println(F("  return {labels, sensorsA, sensorsB, sensorsC, sensorsD};"));
  client.println(F("}"));

  client.println(F("async function pollThreshold() { try { let res = await fetch('/threshold?t=' + new Date().getTime()); let val = parseFloat(await res.text()); if (!isNaN(val) && val !== threshold) { threshold = val; updateThresholdLines(); } } catch(e) {} }"));
  client.println(F("async function pollStatus() { try { let res = await fetch('/status?t=' + new Date().getTime()); let text = await res.text(); updateStatusBar(text.trim().split(',')); } catch(e) {} }"));

  client.println(F("function updateThresholdLines() {"));
  client.println(F("  if (tempChart) {"));
  client.println(F("    let len = tempChart.data.labels.length;"));
  client.println(F("    tempChart.data.datasets[4].data = Array(len).fill(threshold);"));
  client.println(F("    tempChart.data.datasets[5].data = Array(len).fill(threshold + margin);"));
  client.println(F("    tempChart.data.datasets[6].data = Array(len).fill(threshold - margin);"));
  client.println(F("    tempChart.update();"));
  client.println(F("  }"));
  client.println(F("}"));

  client.println(F("async function updateCharts(){"));
  client.println(F("  let rangeT = parseInt(document.getElementById('tempRange').value);"));
  client.println(F("  let rangeH = parseInt(document.getElementById('humidRange').value);"));
  client.println(F("  let tempData = await fetchData('temp.csv', rangeT);"));
  client.println(F("  let humidData = await fetchData('humid.csv', rangeH);"));

  client.println(F("  if(tempChart) tempChart.destroy();"));
  client.println(F("  tempChart = new Chart(document.getElementById('tempChart'), {"));
  client.println(F("    type: 'line', data: { labels: tempData.labels, datasets: ["));
  client.println(F("      {label: 'Sensor A', data: tempData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Sensor B', data: tempData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Sensor C', data: tempData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Sensor D', data: tempData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Threshold', data: Array(tempData.labels.length).fill(threshold), borderColor: 'black', borderDash: [5,5], pointRadius: 0},"));
  client.println(F("      {label: 'High Threshold', data: Array(tempData.labels.length).fill(threshold + margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0},"));
  client.println(F("      {label: 'Low Threshold', data: Array(tempData.labels.length).fill(threshold - margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0}"));
  client.println(F("    ]},"));
  client.println(F("    options: { responsive: true, maintainAspectRatio: false, layout: { padding: { left: 10, right: 20 } }, scales: { x: { type: 'time', time: { tooltipFormat: 'MM/dd/yyyy h:mm a', displayFormats: { hour: 'h:mm a', minute: 'h:mm a', day: 'MMM d' } }, ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } } }, y: { title: { display: true, text: 'Celsius (°C)', font: { size: 13 } }, ticks: { stepSize: 1.0 } } }, interaction: { mode: 'index', intersect: false }, plugins: { tooltip: { mode: 'index', intersect: false }, legend: { labels: { boxWidth: 24, padding: 16, font: { size: 13 } }, onClick: function(e, legendItem, legend) { const index = legendItem.datasetIndex; const ci = legend.chart; if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); } } }, zoom: typeof ChartZoom !== 'undefined' ? { pan: { enabled: true, mode: 'x' }, zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' } } : {} } }"));
  client.println(F("  });"));

  client.println(F("  if(humidChart) humidChart.destroy();"));
  client.println(F("  humidChart = new Chart(document.getElementById('humidChart'), {"));
  client.println(F("    type: 'line', data: { labels: humidData.labels, datasets: ["));
  client.println(F("      {label: 'Sensor A', data: humidData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Sensor B', data: humidData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Sensor C', data: humidData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("      {label: 'Sensor D', data: humidData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4}"));
  client.println(F("    ]},"));
  client.println(F("    options: { responsive: true, maintainAspectRatio: false, layout: { padding: { left: 10, right: 20 } }, scales: { x: { type: 'time', time: { tooltipFormat: 'MM/dd/yyyy h:mm a', displayFormats: { hour: 'h:mm a', minute: 'h:mm a', day: 'MMM d' } }, ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } } }, y: { title: { display: true, text: 'Humidity (%)', font: { size: 13 } }, ticks: { stepSize: 1.0 } } }, interaction: { mode: 'index', intersect: false }, plugins: { tooltip: { mode: 'index', intersect: false }, legend: { labels: { boxWidth: 24, padding: 16, font: { size: 13 } }, onClick: function(e, legendItem, legend) { const index = legendItem.datasetIndex; const ci = legend.chart; if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); } } }, zoom: typeof ChartZoom !== 'undefined' ? { pan: { enabled: true, mode: 'x' }, zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' } } : {} } }"));
  client.println(F("  });"));

  client.println(F("  setTimeout(pollStatus, 500); updateLastUpdate();"));
  client.println(F("}"));

  client.println(F("document.getElementById('tempRange').addEventListener('change', updateCharts);"));
  client.println(F("document.getElementById('humidRange').addEventListener('change', updateCharts);"));
  client.println(F("setInterval(updateCharts, 300000); setInterval(pollThreshold, 30000); setInterval(pollStatus, 30000); setInterval(pollSysInfo, 30000);"));
  client.println(F("pollSysInfo(); updateCharts();"));

  client.println(F("async function pollSysInfo() { try { const res = await fetch('/sysinfo?t=' + new Date().getTime()); const text = await res.text(); const pairs = {}; text.trim().split(',').forEach(p => { const i = p.indexOf(':'); if (i !== -1) pairs[p.substring(0,i).trim()] = p.substring(i+1).trim(); }); const ram = parseInt(pairs['RAM'] || 0); const ramColor = ram > 2000 ? '#2ecc71' : ram > 1000 ? '#f39c12' : '#e74c3c'; const ramEl = document.getElementById('sysRam'); if (ramEl) ramEl.innerHTML = 'RAM: <span style=\"color:' + ramColor + '; font-weight:bold;\">' + (ram/1024).toFixed(1) + ' KB</span>'; const upEl = document.getElementById('sysUptime'); if (upEl) upEl.innerHTML = '&#9201; Uptime: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['UPTIME'] || '--') + '</span>'; const sdEl = document.getElementById('sysSd'); if (sdEl) { sdEl.textContent = 'SD: ' + (pairs['SD'] || '--'); sdEl.style.color = pairs['SD']==='OK' ? '#27ae60' : '#e74c3c'; } const wrEl = document.getElementById('sysWrite'); if (wrEl) wrEl.innerHTML = '&#128221; Last Write: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['LASTWRITE'] || '--') + '</span>'; const ntpEl = document.getElementById('sysNtp'); if (ntpEl) ntpEl.innerHTML = '&#128336; NTP Sync: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['NTPSYNC'] || '--') + '</span>'; } catch(e) {} }"));
  client.println(F("function updateLastUpdate() { document.getElementById('lastUpdate').textContent = new Date().toLocaleString(); }"));

  client.println(F("</script></body></html>"));
}