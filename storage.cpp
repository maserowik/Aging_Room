#include "storage.h"
#include "network.h"
#include "sensors.h"
#include "display.h"

// External LCD reference
extern LiquidCrystal_I2C lcd;

// Free RAM measurement
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
    // We no longer manually create temp.csv here. 
    // The daily files are created dynamically!
  }
}

// Helper function to cleanly rotate the 7-day logs
void prepareDailyFile(const char* filename, String currentDateStr) {
  bool needsRecreate = false;
  
  if (!SD.exists(filename)) {
    needsRecreate = true;
  } else {
    File f = SD.open(filename, FILE_READ);
    if (f) {
      // Skip the header line
      while (f.available()) { if (f.read() == '\n') break; }
      
      // Read the Date from the first row of data
      String fileDate = "";
      while (f.available() && fileDate.length() < 10) {
        char c = f.read();
        if (c == ',' || c == '\n' || c == '\r') break;
        fileDate += c;
      }
      f.close();
      
      // If the file has old data that doesn't match today, it's a week old. Wipe it!
      if (fileDate.length() > 0 && fileDate != currentDateStr) {
        needsRecreate = true;
      }
    } else {
      needsRecreate = true;
    }
  }

  if (needsRecreate) {
    SD.remove(filename);
    File f = SD.open(filename, FILE_WRITE);
    if (f) {
      f.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");
      f.close();
    }
  }
}

void createCsvHeaderIfNeeded() {
  // Legacy function kept to prevent errors in other files, but no longer used.
}

void appendCsvData() {
  extern unsigned long currentEpoch;
  int year, month, day, hour, minute, second, weekday;
  epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);

  // Generate filenames based on the day of the week (0=Sun, 1=Mon... 6=Sat)
  char tFile[10]; snprintf(tFile, sizeof(tFile), "%d_T.csv", weekday);
  char hFile[10]; snprintf(hFile, sizeof(hFile), "%d_H.csv", weekday);

  String dateStr = getDateString();
  String timeStr = getTimeString();

  // Ensure the file exists and is wiped if it's from a week ago
  prepareDailyFile(tFile, dateStr);
  prepareDailyFile(hFile, dateStr);

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

void serveFile(EthernetClient &client, const char *filename, const char *contentType) {
  // The Virtual File Trick: Stitching 7 daily logs together on the fly!
  if (strcmp(filename, "temp.csv") == 0 || strcmp(filename, "humid.csv") == 0) {
    bool isTemp = (strcmp(filename, "temp.csv") == 0);

    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.print("Content-Disposition: inline; filename=\"");
    client.print(filename);
    client.println("\"");
    client.println("Connection: close");
    client.println();

    // Send the Master Header once
    client.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");

    extern unsigned long currentEpoch;
    int year, month, day, hour, minute, second, weekday;
    epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);

    // Stream chronologically: Oldest day to Newest day (today)
    for (int offset = 1; offset <= 7; offset++) {
      int targetDay = (weekday + offset) % 7;
      char fn[10];
      if (isTemp) snprintf(fn, sizeof(fn), "%d_T.csv", targetDay);
      else        snprintf(fn, sizeof(fn), "%d_H.csv", targetDay);

      if (SD.exists(fn)) {
        File f = SD.open(fn, FILE_READ);
        if (f) {
          // Skip the header line in the daily file
          while(f.available()) { if (f.read() == '\n') break; }
          // Stream the raw data blocks
          while(f.available()) { client.write(f.read()); }
          f.close();
        }
      }
    }
    return;
  }

  // Fallback for any other basic files
  if (SD.exists(filename)) {
    File file = SD.open(filename, FILE_READ);
    client.println("HTTP/1.1 200 OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.println("Connection: close");
    client.println();

    while (file.available()) {
      client.write(file.read());
    }
    file.close();
  } else {
    client.println("HTTP/1.1 404 Not Found\nConnection: close\n");
  }
}

void serveThreshold(EthernetClient &client) {
  extern float tempThreshold;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println(tempThreshold, 1);
}

void serveStatus(EthernetClient &client) {
  extern float tA, tB, tC, tD;
  extern float tempThreshold;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
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
      if (temps[i] < tempThreshold - THRESHOLD_MARGIN) {
        client.print("LOW");
      } else if (temps[i] > tempThreshold + THRESHOLD_MARGIN) {
        client.print("HIGH");
      } else {
        client.print("OK");
      }
    }
  }
  client.println();
}

void serveSystemInfo(EthernetClient &client) {
  extern unsigned long lastNtpEpoch;
  extern unsigned long lastCsvWrite;

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();

  int ram = freeMemory();
  client.print("RAM:");
  client.print(ram);
  client.print(",");

  unsigned long ms = millis();
  unsigned long secs  = ms / 1000;
  unsigned long mins  = secs / 60;
  unsigned long hours = mins / 60;
  unsigned long days  = hours / 24;
  client.print("UPTIME:");
  client.print(days);
  client.print("d ");
  client.print(hours % 24);
  client.print("h ");
  client.print(mins % 60);
  client.print("m,");

  client.print("SD:");
  client.print(SD.begin(SD_CHIP_SELECT) ? "OK" : "FAIL");
  client.print(",");

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
    client.print("LASTWRITE:");
    client.print(buf);
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
    client.print("NTPSYNC:");
    client.print(buf);
  }
  client.println();
}

void serveRootPage(EthernetClient &client) {
  String lastUpdate = getDateString() + " " + getTimeString();

  extern float tempThreshold;
  extern float tA, tB, tC, tD;

  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<title>Seegrid Aging Room Data</title>"));
  client.println(F("<style>"));
  client.println(F("body{font-family:sans-serif;background:#f4f4f4;padding:12px 16px;box-sizing:border-box;margin:0;}"));
  client.println(F(".tab{display:inline-block;padding:10px 20px;margin:5px;background:#ccc;cursor:pointer;}"));
  client.println(F(".tab.active{background:#999;}"));
  client.println(F(".tab-content{display:none;}"));
  client.println(F(".tab-content.active{display:block;}"));
  client.println(F("button{margin-left:10px;}"));
  client.println(F(".chart-scroll-wrapper{width:100%;background:#fff;}"));
  client.println(F(".chart-scroll-wrapper canvas{height:500px !important;display:block;}"));
  client.println(F("#statusBar{display:flex;gap:20px;align-items:center;padding:12px 16px;margin-bottom:12px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:15px;flex-wrap:wrap;}"));
  client.println(F(".sensor-dot{display:inline-block;width:14px;height:14px;border-radius:50%;margin-right:6px;vertical-align:middle;}"));
  
  client.println(F("@keyframes errorBlink { 0% { opacity: 1; } 50% { opacity: 0; } 100% { opacity: 1; } }"));
  
  client.println(F(".dot-warn{background:#f39c12;}"));
  client.println(F(".dot-err{background:#e74c3c; animation: errorBlink 1s infinite;}"));
  
  client.println(F(".sensor-label{font-weight:bold;}"));
  client.println(F(".sensor-warn{color:#f39c12;}"));
  client.println(F(".sensor-err{color:#e74c3c; animation: errorBlink 1s infinite;}"));
  
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

  client.println(F("<div id='statusBar'>"));
  client.println(F("  <strong>Sensors:</strong>"));

  const char* sensorLabels[] = {"A", "B", "C", "D"};
  const char* sensorIdClasses[] = {"s-a", "s-b", "s-c", "s-d"};
  float initTemps[] = {tA, tB, tC, tD};
  for (int i = 0; i < 4; i++) {
    bool isErr = isnan(initTemps[i]);
    bool isLow = !isErr && initTemps[i] < tempThreshold - THRESHOLD_MARGIN;
    bool isHigh = !isErr && initTemps[i] > tempThreshold + THRESHOLD_MARGIN;
    bool isOk = !isErr && !isLow && !isHigh;

    client.print(F("  <span id='statusSensor"));
    client.print(sensorLabels[i]);
    client.print(F("'><span class='sensor-dot "));
    client.print(sensorIdClasses[i]);
    if (isErr)      client.print(F(" dot-err"));
    else if (!isOk) client.print(F(" dot-warn"));
    client.print(F("'></span><span class='sensor-label "));
    client.print(sensorIdClasses[i]);
    if (isErr)      client.print(F(" sensor-err"));
    else if (!isOk) client.print(F(" sensor-warn"));
    client.print(F("'>"));
    client.print(sensorLabels[i]);
    if (isErr) {
      client.print(F(" ERR"));
    } else {
      float tempC = initTemps[i];
      float tempF = tempC * 9.0 / 5.0 + 32.0;
      client.print(F("</span><span class='sensor-temp'>"));
      client.print(tempC, 1);
      client.print(F("°C ("));
      client.print(tempF, 1);
      client.print(F("°F)"));
      if (isLow)       client.print(F(" ↓ LOW"));
      else if (isHigh) client.print(F(" ↑ HIGH"));
      else             client.print(F(" ✓"));
    }
    client.print(F("</span></span>  "));
  }
  client.println(F("</div>"));

  client.println(F("<div>"));
  client.println(F("<div class='tab active' onclick=\"showTab('temp', event)\">Temperature</div>"));
  client.println(F("<div class='tab' onclick=\"showTab('humid', event)\">Humidity</div>"));
  client.println(F("</div>"));

  // Delete buttons have been removed since rotation is automatic!
  client.println(F("<div id='temp' class='tab-content active'>"));
  client.println(F("<label>Range: <select id='tempRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(tempChart, \"temp\")'>Export Temperature PNG</button>"));
  client.println(F("<button onclick=\"window.location='/temp.csv'\">Download Temperature CSV</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(tempChart&&tempChart.resetZoom)tempChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><div class='chart-scroll-wrapper'><canvas id='tempChart'></canvas></div></div>"));

  client.println(F("<div id='humid' class='tab-content'>"));
  client.println(F("<label>Range: <select id='humidRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(humidChart, \"humid\")'>Export Humidity PNG</button>"));
  client.println(F("<button onclick=\"window.location='/humid.csv'\">Download Humidity CSV</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(humidChart&&humidChart.resetZoom)humidChart.resetZoom()'>Reset Zoom</button>"));
  client.println(F("<br><div class='chart-scroll-wrapper'><canvas id='humidChart'></canvas></div></div>"));

  client.println(F("<div id='sysPanel' style='margin-top:16px;padding:12px 16px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:14px;'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:8px;font-size:15px;'>System Status</div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;'>"));
  client.println(F("<span id='sysRam'>RAM: --</span>"));
  client.println(F("<span id='sysUptime'>&#9201; Uptime: --</span>"));
  client.println(F("<span id='sysSd'>&#128190; SD: --</span>"));
  client.println(F("</div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;margin-top:6px;'>"));
  client.println(F("<span id='sysWrite'>&#128221; Last Write: --</span>"));
  client.println(F("<span id='sysNtp'>&#128336; NTP Sync: --</span>"));
  client.println(F("</div>"));
  client.println(F("</div>"));

  client.println(F("<script>"));
  client.println(F("let tempChart, humidChart;"));

  client.print(F("let threshold = "));
  client.print(tempThreshold, 1);
  client.println(F(";"));
  client.println(F("const margin = 5.0;"));

  client.println(F("function showTab(id, evt){"));
  client.println(F("  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));"));
  client.println(F("  document.querySelectorAll('.tab-content').forEach(c=>c.classList.remove('active'));"));
  client.println(F("  document.getElementById(id).classList.add('active');"));
  client.println(F("  evt.target.classList.add('active');"));
  client.println(F("}"));

  client.println(F("function downloadChart(chart, label){"));
  client.println(F("  if(!chart) return;"));
  client.println(F("  const link = document.createElement('a');"));
  client.println(F("  link.download = label + '_chart.png';"));
  client.println(F("  link.href = chart.toBase64Image();"));
  client.println(F("  link.click();"));
  client.println(F("}"));

  client.println(F("const sensorMap = {"));
  client.println(F("  'A': {idClass:'s-a', color:'#0072B2'},"));
  client.println(F("  'B': {idClass:'s-b', color:'#E69F00'},"));
  client.println(F("  'C': {idClass:'s-c', color:'#CC79A7'},"));
  client.println(F("  'D': {idClass:'s-d', color:'#56B4E9'}"));
  client.println(F("};"));

  client.println(F("function updateStatusBar(records) {"));
  client.println(F("  records.forEach(rec => {"));
  client.println(F("    const colonIdx = rec.indexOf(':');"));
  client.println(F("    if (colonIdx === -1) return;"));
  client.println(F("    const label = rec.substring(0, colonIdx).trim();"));
  client.println(F("    const rest  = rec.substring(colonIdx + 1).trim();"));
  client.println(F("    const parts = rest.split('|');"));
  client.println(F("    const tempC = parts[0];"));
  client.println(F("    const state = parts[1];"));
  client.println(F("    const isErr  = state === 'ERR';"));
  client.println(F("    const isOk   = state === 'OK';"));
  client.println(F("    const isLow  = state === 'LOW';"));
  client.println(F("    const isHigh = state === 'HIGH';"));
  client.println(F("    const info = sensorMap[label];"));
  client.println(F("    if (!info) return;"));
  client.println(F("    const el = document.getElementById('statusSensor' + label);"));
  client.println(F("    if (el) {"));
  client.println(F("      const dot = el.querySelector('.sensor-dot');"));
  client.println(F("      dot.className = 'sensor-dot ' + info.idClass + (isErr ? ' dot-err' : !isOk ? ' dot-warn' : '');"));
  client.println(F("      const lbl = el.querySelector('.sensor-label');"));
  client.println(F("      lbl.className = 'sensor-label ' + info.idClass + (isErr ? ' sensor-err' : !isOk ? ' sensor-warn' : '');"));
  client.println(F("      if (isErr) {"));
  client.println(F("        lbl.textContent = label + ' ERR';"));
  client.println(F("        const tmp = el.querySelector('.sensor-temp');"));
  client.println(F("        if (tmp) tmp.textContent = '';"));
  client.println(F("      } else {"));
  client.println(F("        lbl.textContent = label;"));
  client.println(F("        const cVal = parseFloat(tempC);"));
  client.println(F("        const fVal = (cVal * 9 / 5 + 32).toFixed(1);"));
  client.println(F("        const suffix = isLow ? ' \u2193 LOW' : isHigh ? ' \u2191 HIGH' : ' \u2713';"));
  client.println(F("        const tmp = el.querySelector('.sensor-temp');"));
  client.println(F("        if (tmp) tmp.textContent = cVal.toFixed(1) + '\u00b0C (' + fVal + '\u00b0F)' + suffix;"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("    if (tempChart) {"));
  client.println(F("      const ds = tempChart.data.datasets.find(d => d.label === 'Sensor ' + label);"));
  client.println(F("      if (ds) {"));
  client.println(F("        ds.borderColor = isErr ? '#e74c3c' : info.color;"));
  client.println(F("        ds.backgroundColor = ds.borderColor;"));
  client.println(F("        ds.pointBackgroundColor = ds.borderColor;"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("  });"));
  client.println(F("  if (tempChart) tempChart.update();"));
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
  client.println(F("    let dtStr = date.split('-').join('/') + ' ' + time;"));
  client.println(F("    let dt = new Date(dtStr);"));
  client.println(F("    if(dt.getTime() >= limit){"));
  client.println(F("      labels.push(dt.getTime());"));
  client.println(F("      sensorsA.push(parseFloat(a) || null);"));
  client.println(F("      sensorsB.push(parseFloat(b) || null);"));
  client.println(F("      sensorsC.push(parseFloat(c) || null);"));
  client.println(F("      sensorsD.push(parseFloat(d) || null);"));
  client.println(F("    }"));
  client.println(F("  });"));
  client.println(F("  return {labels, sensorsA, sensorsB, sensorsC, sensorsD};"));
  client.println(F("}"));

  client.println(F("async function pollThreshold() {"));
  client.println(F("  try {"));
  client.println(F("    let res = await fetch('/threshold?t=' + new Date().getTime());"));
  client.println(F("    let val = parseFloat(await res.text());"));
  client.println(F("    if (!isNaN(val) && val !== threshold) {"));
  client.println(F("      threshold = val;"));
  client.println(F("      updateThresholdLines();"));
  client.println(F("    }"));
  client.println(F("  } catch(e) {}"));
  client.println(F("}"));

  client.println(F("async function pollStatus() {"));
  client.println(F("  try {"));
  client.println(F("    let res = await fetch('/status?t=' + new Date().getTime());"));
  client.println(F("    let text = await res.text();"));
  client.println(F("    updateStatusBar(text.trim().split(','));"));
  client.println(F("  } catch(e) {}"));
  client.println(F("}"));

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
  client.println(F("    type: 'line',"));
  client.println(F("    data: {"));
  client.println(F("      labels: tempData.labels,"));
  client.println(F("      datasets: ["));
  client.println(F("        {label: 'Sensor A', data: tempData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: tempData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: tempData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: tempData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Threshold', data: Array(tempData.labels.length).fill(threshold), borderColor: 'black', borderDash: [5,5], pointRadius: 0},"));
  client.println(F("        {label: 'High Threshold', data: Array(tempData.labels.length).fill(threshold + margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0},"));
  client.println(F("        {label: 'Low Threshold', data: Array(tempData.labels.length).fill(threshold - margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0}"));
  client.println(F("      ]"));
  client.println(F("    },"));
  client.println(F("    options: {"));
  client.println(F("      responsive: true,"));
  client.println(F("      maintainAspectRatio: false,"));
  client.println(F("      layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: {"));
  client.println(F("        x: {"));
  client.println(F("          type: 'time',"));
  client.println(F("          time: { tooltipFormat: 'MM/dd/yyyy h:mm a', displayFormats: { hour: 'h:mm a', minute: 'h:mm a', day: 'MMM d' } },"));
  client.println(F("          ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } }"));
  client.println(F("        },"));
  client.println(F("        y: {"));
  client.println(F("          title: { display: true, text: 'Celsius (°C)', font: { size: 13 } },"));
  client.println(F("          ticks: { stepSize: 1.0 }"));
  client.println(F("        }"));
  client.println(F("      },"));
  client.println(F("      interaction: { mode: 'index', intersect: false },"));
  client.println(F("      plugins: {"));
  client.println(F("        tooltip: { mode: 'index', intersect: false },"));
  client.println(F("        legend: {"));
  client.println(F("          labels: { boxWidth: 24, padding: 16, font: { size: 13 } },"));
  client.println(F("          onClick: function(e, legendItem, legend) {"));
  client.println(F("            const index = legendItem.datasetIndex;"));
  client.println(F("            const ci = legend.chart;"));
  client.println(F("            if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); }"));
  client.println(F("          }"));
  client.println(F("        },"));
  client.println(F("        zoom: typeof ChartZoom !== 'undefined' ? {"));
  client.println(F("          pan: { enabled: true, mode: 'x' },"));
  client.println(F("          zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' }"));
  client.println(F("        } : {}"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("  });"));

  client.println(F("  if(humidChart) humidChart.destroy();"));
  client.println(F("  humidChart = new Chart(document.getElementById('humidChart'), {"));
  client.println(F("    type: 'line',"));
  client.println(F("    data: {"));
  client.println(F("      labels: humidData.labels,"));
  client.println(F("      datasets: ["));
  client.println(F("        {label: 'Sensor A', data: humidData.sensorsA, borderColor: '#0072B2', backgroundColor: '#0072B2', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: humidData.sensorsB, borderColor: '#E69F00', backgroundColor: '#E69F00', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: humidData.sensorsC, borderColor: '#CC79A7', backgroundColor: '#CC79A7', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: humidData.sensorsD, borderColor: '#56B4E9', backgroundColor: '#56B4E9', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4}"));
  client.println(F("      ]"));
  client.println(F("    },"));
  client.println(F("    options: {"));
  client.println(F("      responsive: true,"));
  client.println(F("      maintainAspectRatio: false,"));
  client.println(F("      layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: {"));
  client.println(F("        x: {"));
  client.println(F("          type: 'time',"));
  client.println(F("          time: { tooltipFormat: 'MM/dd/yyyy h:mm a', displayFormats: { hour: 'h:mm a', minute: 'h:mm a', day: 'MMM d' } },"));
  client.println(F("          ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } }"));
  client.println(F("        },"));
  client.println(F("        y: { title: { display: true, text: 'Humidity (%)', font: { size: 13 } }, ticks: { stepSize: 1.0 } }"));
  client.println(F("      },"));
  client.println(F("      interaction: { mode: 'index', intersect: false },"));
  client.println(F("      plugins: {"));
  client.println(F("        tooltip: { mode: 'index', intersect: false },"));
  client.println(F("        legend: {"));
  client.println(F("          labels: { boxWidth: 24, padding: 16, font: { size: 13 } },"));
  client.println(F("          onClick: function(e, legendItem, legend) {"));
  client.println(F("            const index = legendItem.datasetIndex;"));
  client.println(F("            const ci = legend.chart;"));
  client.println(F("            if (ci.isDatasetVisible(index)) { ci.hide(index); } else { ci.show(index); }"));
  client.println(F("          }"));
  client.println(F("        },"));
  client.println(F("        zoom: typeof ChartZoom !== 'undefined' ? {"));
  client.println(F("          pan: { enabled: true, mode: 'x' },"));
  client.println(F("          zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' }"));
  client.println(F("        } : {}"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("  });"));

  client.println(F("  setTimeout(pollStatus, 500);"));
  client.println(F("  updateLastUpdate();"));
  client.println(F("}"));

  client.println(F("document.getElementById('tempRange').addEventListener('change', updateCharts);"));
  client.println(F("document.getElementById('humidRange').addEventListener('change', updateCharts);"));
  client.println(F("setInterval(updateCharts, 300000);"));
  client.println(F("setInterval(pollThreshold, 30000);"));
  client.println(F("setInterval(pollStatus, 30000);"));
  client.println(F("setInterval(pollSysInfo, 30000);"));
  client.println(F("pollSysInfo();"));
  client.println(F("updateCharts();"));

  client.println(F("async function pollSysInfo() {"));
  client.println(F("  try {"));
  client.println(F("    const res = await fetch('/sysinfo?t=' + new Date().getTime());"));
  client.println(F("    const text = await res.text();"));
  client.println(F("    const pairs = {};"));
  client.println(F("    text.trim().split(',').forEach(p => {"));
  client.println(F("      const i = p.indexOf(':');"));
  client.println(F("      if (i !== -1) pairs[p.substring(0,i).trim()] = p.substring(i+1).trim();"));
  client.println(F("    });"));
  client.println(F("    const ram = parseInt(pairs['RAM'] || 0);"));
  client.println(F("    const ramColor = ram > 2000 ? '#2ecc71' : ram > 1000 ? '#f39c12' : '#e74c3c';"));
  client.println(F("    const ramEl = document.getElementById('sysRam');"));
  client.println(F("    if (ramEl) ramEl.innerHTML = 'RAM: <span style=\"color:' + ramColor + '; font-weight:bold;\">' + (ram/1024).toFixed(1) + ' KB</span>';"));
  client.println(F("    const upEl = document.getElementById('sysUptime');"));
  client.println(F("    if (upEl) upEl.innerHTML = '&#9201; Uptime: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['UPTIME'] || '--') + '</span>';"));
  client.println(F("    const sdEl = document.getElementById('sysSd');"));
  client.println(F("    if (sdEl) { sdEl.textContent = 'SD: ' + (pairs['SD'] || '--'); sdEl.style.color = pairs['SD']==='OK' ? '#27ae60' : '#e74c3c'; }"));
  client.println(F("    const wrEl = document.getElementById('sysWrite');"));
  client.println(F("    if (wrEl) wrEl.innerHTML = '&#128221; Last Write: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['LASTWRITE'] || '--') + '</span>';"));
  client.println(F("    const ntpEl = document.getElementById('sysNtp');"));
  client.println(F("    if (ntpEl) ntpEl.innerHTML = '&#128336; NTP Sync: <span style=\"color:#27ae60; font-weight:bold;\">' + (pairs['NTPSYNC'] || '--') + '</span>';"));
  client.println(F("  } catch(e) {}"));
  client.println(F("}"));

  client.println(F("function updateLastUpdate() {"));
  client.println(F("  const now = new Date();"));
  client.println(F("  const formatted = now.toLocaleString();"));
  client.println(F("  document.getElementById('lastUpdate').textContent = formatted;"));
  client.println(F("}"));

  client.println(F("</script>"));

  client.println(F("</body></html>"));
}