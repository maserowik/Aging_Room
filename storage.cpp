#include "storage.h"
#include "network.h"
#include "sensors.h"
#include "display.h"

// External LCD reference — declared here at file scope, not inside functions
extern LiquidCrystal_I2C lcd;

// Free RAM measurement for Arduino Mega
// Measures gap between heap top and stack bottom
int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// Global timing variable
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
    createCsvHeaderIfNeeded();
  }
}

void createCsvHeaderIfNeeded() {
  if (!SD.exists("temp.csv")) {
    File f = SD.open("temp.csv", FILE_WRITE);
    if (f) {
      f.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");
      f.close();
    }
  }
  if (!SD.exists("humid.csv")) {
    File f = SD.open("humid.csv", FILE_WRITE);
    if (f) {
      f.println("Date,Time,Sensor A,Sensor B,Sensor C,Sensor D");
      f.close();
    }
  }
}

void appendCsvData() {
  String dateStr = getDateString();
  String timeStr = getTimeString();

  extern float tA, tB, tC, tD, hA, hB, hC, hD;

  File tf = SD.open("temp.csv", FILE_WRITE);
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

    Serial.print("Temperature data written to temp.csv: ");
    Serial.print(dateStr);
    Serial.print(", ");
    Serial.print(timeStr);
    Serial.print(", ");
    Serial.print(isnan(tA) ? "ERR" : String(tA, 1) + " C");
    Serial.print(", ");
    Serial.print(isnan(tB) ? "ERR" : String(tB, 1) + " C");
    Serial.print(", ");
    Serial.print(isnan(tC) ? "ERR" : String(tC, 1) + " C");
    Serial.print(", ");
    Serial.println(isnan(tD) ? "ERR" : String(tD, 1) + " C");
  } else {
    Serial.println("Failed to open temp.csv for writing.");
  }

  File hf = SD.open("humid.csv", FILE_WRITE);
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

    Serial.print("Humidity data written to humid.csv: ");
    Serial.print(dateStr);
    Serial.print(", ");
    Serial.print(timeStr);
    Serial.print(", ");
    Serial.print(isnan(hA) ? "ERR" : String(hA, 1) + " %");
    Serial.print(", ");
    Serial.print(isnan(hB) ? "ERR" : String(hB, 1) + " %");
    Serial.print(", ");
    Serial.print(isnan(hC) ? "ERR" : String(hC, 1) + " %");
    Serial.print(", ");
    Serial.println(isnan(hD) ? "ERR" : String(hD, 1) + " %");
  } else {
    Serial.println("Failed to open humid.csv for writing.");
  }
}

void serveFile(EthernetClient &client, const char *filename, const char *contentType) {
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
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println("File not found");
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
  // Format per sensor: LABEL:TEMP_C|STATE separated by commas
  // Includes sensor label so JS can look up by ID, not by position
  // STATE is OK, LOW, HIGH, or ERR
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

  // Free RAM
  int ram = freeMemory();
  client.print("RAM:");
  client.print(ram);
  client.print(",");

  // Uptime from millis()
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

  // SD card status
  client.print("SD:");
  client.print(SD.begin(SD_CHIP_SELECT) ? "OK" : "FAIL");
  client.print(",");

  // Last CSV write — convert millis offset to real time
  if (lastCsvWrite == 0) {
    client.print("LASTWRITE:Never");
  } else {
    // Calculate epoch at last write:
    // currentEpoch is now, millis() is now, lastCsvWrite is millis at write time
    extern unsigned long currentEpoch;
    unsigned long secsAgo = (millis() - lastCsvWrite) / 1000;
    unsigned long writeEpoch = currentEpoch - secsAgo;
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(writeEpoch, y, mo, d, h, mi, s, wd);
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d-%02d-%04d %02d:%02d:%02d", mo+1, d, y, h, mi, s);
    client.print("LASTWRITE:");
    client.print(buf);
  }
  client.print(",");

  // NTP last sync
  if (lastNtpEpoch == 0) {
    client.print("NTPSYNC:Never");
  } else {
    int y, mo, d, h, mi, s, wd;
    epochToDateTime(lastNtpEpoch, y, mo, d, h, mi, s, wd);
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d-%02d-%04d %02d:%02d:%02d", mo+1, d, y, h, mi, s);
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
  // FIX 1: Scrollable chart wrapper — replaces fixed canvas rule
  client.println(F("button{margin-left:10px;}"));
  client.println(F(".chart-scroll-wrapper{width:100%;background:#fff;}"));
  client.println(F(".chart-scroll-wrapper canvas{height:500px !important;display:block;}"));
  // Status bar styles
  client.println(F("#statusBar{display:flex;gap:20px;align-items:center;padding:12px 16px;margin-bottom:12px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:15px;flex-wrap:wrap;}"));
  client.println(F(".sensor-dot{display:inline-block;width:14px;height:14px;border-radius:50%;margin-right:6px;vertical-align:middle;}"));
  client.println(F(".dot-warn{background:#f39c12;}"));
  client.println(F(".dot-err{background:#e74c3c;}"));
  client.println(F(".sensor-label{font-weight:bold;}"));
  client.println(F(".sensor-warn{color:#f39c12;}"));
  client.println(F(".sensor-err{color:#e74c3c;}"));
  client.println(F(".sensor-temp{color:#555;font-size:14px;margin-left:4px;}"));
  // FIX 2: Per-sensor identity colors matching chart line colors
  client.println(F(".sensor-dot.s-a{background:red;} .sensor-label.s-a{color:red;}"));
  client.println(F(".sensor-dot.s-b{background:blue;} .sensor-label.s-b{color:blue;}"));
  client.println(F(".sensor-dot.s-c{background:green;} .sensor-label.s-c{color:green;}"));
  client.println(F(".sensor-dot.s-d{background:orange;} .sensor-label.s-d{color:orange;}"));
  client.println(F("</style>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/hammerjs@2.0.8/hammer.min.js'></script>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@1.2.1/dist/chartjs-plugin-zoom.min.js'></script>"));
  client.println(F("</head><body>"));

  client.println(F("<h2>Seegrid Aging Room Data</h2>"));
  client.print(F("<p>Last update: <span id='lastUpdate'>"));
  client.print(lastUpdate);
  client.println(F("</span></p>"));

  // Sensor status bar — initial render, server-side
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
    // Dot: identity class always present; state class added on top when needed
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

  client.println(F("<div id='temp' class='tab-content active'>"));
  client.println(F("<label>Range: <select id='tempRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(tempChart, \"temp\")'>Export Temperature PNG</button>"));
  client.println(F("<button onclick=\"window.location='/temp.csv'\">Download Temperature CSV</button>"));
  client.println(F("<button onclick='confirmDelete(\"temp\")'>Delete Temperature CSV</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(tempChart&&tempChart.resetZoom)tempChart.resetZoom()'>Reset Zoom</button>"));
  // FIX 1: chart wrapped in scroll div
  client.println(F("<br><div class='chart-scroll-wrapper'><canvas id='tempChart'></canvas></div></div>"));

  client.println(F("<div id='humid' class='tab-content'>"));
  client.println(F("<label>Range: <select id='humidRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(humidChart, \"humid\")'>Export Humidity PNG</button>"));
  client.println(F("<button onclick=\"window.location='/humid.csv'\">Download Humidity CSV</button>"));
  client.println(F("<button onclick='confirmDelete(\"humid\")'>Delete Humidity CSV</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<button onclick='if(humidChart&&humidChart.resetZoom)humidChart.resetZoom()'>Reset Zoom</button>"));
  // FIX 1: chart wrapped in scroll div
  client.println(F("<br><div class='chart-scroll-wrapper'><canvas id='humidChart'></canvas></div></div>"));

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

  client.println(F("function confirmDelete(type) {"));
  client.println(F("  if(confirm('Are you sure you want to delete the ' + type + ' CSV file?')) {"));
  client.println(F("    window.location = '/delete_' + type;"));
  client.println(F("  }"));
  client.println(F("}"));

  // FIX 3: updateStatusBar keyed by sensor label, NOT array index.
  // serveStatus now prefixes each record with "A:", "B:", etc.
  // This means legend hide/show never corrupts the status bar.
  client.println(F("const sensorMap = {"));
  client.println(F("  'A': {idClass:'s-a', color:'red'},"));
  client.println(F("  'B': {idClass:'s-b', color:'blue'},"));
  client.println(F("  'C': {idClass:'s-c', color:'green'},"));
  client.println(F("  'D': {idClass:'s-d', color:'orange'}"));
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
  // Look up by fixed element ID — immune to chart dataset index shifts
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
  // Chart lines keep sensor identity color always — only ERR turns line red
  // LOW/HIGH only affects the status bar dot/label, not the chart line color
  client.println(F("    if (tempChart) {"));
  client.println(F("      const ds = tempChart.data.datasets.find(d => d.label === 'Sensor ' + label);"));
  client.println(F("      if (ds) {"));
  client.println(F("        ds.borderColor = isErr ? '#e74c3c' : info.color;"));
  client.println(F("        ds.backgroundColor = ds.borderColor;"));
  client.println(F("        ds.pointBackgroundColor = ds.borderColor;"));
  client.println(F("      }"));
  client.println(F("    }"));
  client.println(F("  });"));
  // Update chart once after all sensors processed, not once per sensor
  client.println(F("  if (tempChart) tempChart.update();"));
  client.println(F("}"));

  client.println(F("async function fetchData(filename, rangeDays) {"));
  client.println(F("  let res = await fetch('/' + filename);"));
  client.println(F("  let text = await res.text();"));
  client.println(F("  let lines = text.trim().split('\\n').slice(1);"));
  client.println(F("  let limit = new Date().getTime() - rangeDays * 86400000;"));
  client.println(F("  let labels=[], sensorsA=[], sensorsB=[], sensorsC=[], sensorsD=[];"));
  client.println(F("  let downsampleRate = rangeDays <= 1 ? 1 : rangeDays <= 3 ? 6 : 12;"));
  client.println(F("  lines.forEach((line, idx) => {"));
  client.println(F("    if (idx % downsampleRate !== 0) return;"));
  client.println(F("    let [date, time, a, b, c, d] = line.split(',');"));
  client.println(F("    let dt = new Date(date + ' ' + time);"));
  client.println(F("    if(dt.getTime() >= limit){"));
  client.println(F("      labels.push(date + ' ' + time);"));
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
  client.println(F("    let res = await fetch('/threshold');"));
  client.println(F("    let val = parseFloat(await res.text());"));
  client.println(F("    if (!isNaN(val) && val !== threshold) {"));
  client.println(F("      threshold = val;"));
  client.println(F("      updateThresholdLines();"));
  client.println(F("    }"));
  client.println(F("  } catch(e) {}"));
  client.println(F("}"));

  client.println(F("async function pollStatus() {"));
  client.println(F("  try {"));
  client.println(F("    let res = await fetch('/status');"));
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
  client.println(F("        {label: 'Sensor A', data: tempData.sensorsA, borderColor: 'red', backgroundColor: 'red', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: tempData.sensorsB, borderColor: 'blue', backgroundColor: 'blue', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: tempData.sensorsC, borderColor: 'green', backgroundColor: 'green', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: tempData.sensorsD, borderColor: 'orange', backgroundColor: 'orange', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
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
  client.println(F("          ticks: {"));
  client.println(F("            maxRotation: 45,"));
  client.println(F("            minRotation: 45,"));
  client.println(F("            maxTicksLimit: 24,"));
  client.println(F("            font: { size: 10 }"));
  client.println(F("          }"));
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
  client.println(F("        {label: 'Sensor A', data: humidData.sensorsA, borderColor: 'red', backgroundColor: 'red', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor B', data: humidData.sensorsB, borderColor: 'blue', backgroundColor: 'blue', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor C', data: humidData.sensorsC, borderColor: 'green', backgroundColor: 'green', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4},"));
  client.println(F("        {label: 'Sensor D', data: humidData.sensorsD, borderColor: 'orange', backgroundColor: 'orange', fill: false, borderWidth: 2, pointRadius: 0, pointHoverRadius: 4}"));
  client.println(F("      ]"));
  client.println(F("    },"));
  client.println(F("    options: {"));
  client.println(F("      responsive: true,"));
  client.println(F("      maintainAspectRatio: false,"));
  client.println(F("      layout: { padding: { left: 10, right: 20 } },"));
  client.println(F("      scales: {"));
  client.println(F("        x: { ticks: { maxRotation: 45, minRotation: 45, maxTicksLimit: 24, font: { size: 10 } } },"));
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
  client.println(F("    const res = await fetch('/sysinfo');"));
  client.println(F("    const text = await res.text();"));
  client.println(F("    const pairs = {};"));
  client.println(F("    text.trim().split(',').forEach(p => {"));
  client.println(F("      const i = p.indexOf(':');"));
  client.println(F("      if (i !== -1) pairs[p.substring(0,i).trim()] = p.substring(i+1).trim();"));
  client.println(F("    });"));
  // RAM with color indicator
  client.println(F("    const ram = parseInt(pairs['RAM'] || 0);"));
  client.println(F("    const ramColor = ram > 2000 ? '#2ecc71' : ram > 1000 ? '#f39c12' : '#e74c3c';"));
  client.println(F("    const ramEl = document.getElementById('sysRam');"));
  client.println(F("    if (ramEl) ramEl.innerHTML = '<span class=\'sys-dot\' style=\'background:' + ramColor + '\'></span>RAM: ' + (ram/1024).toFixed(1) + ' KB';"));
  // Uptime
  client.println(F("    const upEl = document.getElementById('sysUptime');"));
  client.println(F("    if (upEl) upEl.textContent = 'Uptime: ' + (pairs['UPTIME'] || '--');"));
  // SD
  client.println(F("    const sdEl = document.getElementById('sysSd');"));
  client.println(F("    if (sdEl) { sdEl.textContent = 'SD: ' + (pairs['SD'] || '--'); sdEl.style.color = pairs['SD']==='OK' ? '#27ae60' : '#e74c3c'; }"));
  // Last write
  client.println(F("    const wrEl = document.getElementById('sysWrite');"));
  client.println(F("    if (wrEl) wrEl.textContent = 'Last Write: ' + (pairs['LASTWRITE'] || '--');"));
  // NTP sync
  client.println(F("    const ntpEl = document.getElementById('sysNtp');"));
  client.println(F("    if (ntpEl) ntpEl.textContent = 'NTP Sync: ' + (pairs['NTPSYNC'] || '--');"));
  client.println(F("  } catch(e) {}"));
  client.println(F("}"));

  client.println(F("function updateLastUpdate() {"));
  client.println(F("  const now = new Date();"));
  client.println(F("  const formatted = now.toLocaleString();"));
  client.println(F("  document.getElementById('lastUpdate').textContent = formatted;"));
  client.println(F("}"));

  client.println(F("</script>"));

  // System status panel — HTML must be OUTSIDE the script tag
  client.println(F("<div id='sysPanel' style='margin-top:16px;padding:12px 16px;background:#fff;border-radius:6px;border:1px solid #ddd;font-size:14px;'>"));
  client.println(F("<div style='font-weight:bold;margin-bottom:8px;font-size:15px;'>System Status</div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;'>"));
  client.println(F("<span id='sysRam'><span class='sys-dot' style='background:#2ecc71;'></span>RAM: --</span>"));
  client.println(F("<span id='sysUptime'>&#9201; Uptime: --</span>"));
  client.println(F("<span id='sysSd'>&#128190; SD: --</span>"));
  client.println(F("</div>"));
  client.println(F("<div style='display:flex;flex-wrap:wrap;gap:20px;margin-top:6px;'>"));
  client.println(F("<span id='sysWrite'>&#128221; Last Write: --</span>"));
  client.println(F("<span id='sysNtp'>&#128336; NTP Sync: --</span>"));
  client.println(F("</div>"));
  client.println(F("</div>"));

  client.println(F("</body></html>"));
}
