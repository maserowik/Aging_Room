#include "storage.h"
#include "network.h"
#include "sensors.h"
#include "display.h"

// Global timing variable
unsigned long lastCsvWrite = 0;

void initSDCard() {
  if (!SD.begin(SD_CHIP_SELECT)) {
    Serial.println("SD card initialization failed!");
    extern LiquidCrystal_I2C lcd;
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

void serveRootPage(EthernetClient &client) {
  String lastUpdate = getDateString() + " " + getTimeString();

  extern float tempThreshold;

  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));
  client.println();

  client.println(F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"));
  client.println(F("<title>Seegrid Aging Room Data</title>"));
  client.println(F("<style>"));
  client.println(F("body{font-family:sans-serif;background:#f4f4f4;padding:20px;}"));
  client.println(F(".tab{display:inline-block;padding:10px 20px;margin:5px;background:#ccc;cursor:pointer;}"));
  client.println(F(".tab.active{background:#999;}"));
  client.println(F(".tab-content{display:none;}"));
  client.println(F(".tab-content.active{display:block;}"));
  client.println(F("canvas { width: 100% !important; height: auto !important; }"));
  client.println(F("button { margin-left: 10px; }"));
  client.println(F("</style>"));
  client.println(F("<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"));
  client.println(F("</head><body>"));

  client.println(F("<h2>Seegrid Aging Room Data</h2>"));
  client.print(F("<p>Last update: <span id='lastUpdate'>"));
  client.print(lastUpdate);
  client.println(F("</span></p>"));

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
  client.println(F("<br><div style='max-width:1000px; height:600px;'><canvas id='tempChart'></canvas></div></div>"));

  client.println(F("<div id='humid' class='tab-content'>"));
  client.println(F("<label>Range: <select id='humidRange'><option selected>1</option><option>3</option><option>5</option><option>7</option></select> days</label>"));
  client.println(F("<button onclick='downloadChart(humidChart, \"humid\")'>Export Humidity PNG</button>"));
  client.println(F("<button onclick=\"window.location='/humid.csv'\">Download Humidity CSV</button>"));
  client.println(F("<button onclick='confirmDelete(\"humid\")'>Delete Humidity CSV</button>"));
  client.println(F("<button onclick='updateCharts()'>Update Now</button>"));
  client.println(F("<br><div style='max-width:1000px; height:600px;'><canvas id='humidChart'></canvas></div></div>"));

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

  client.println(F("async function fetchData(filename, rangeDays) {"));
  client.println(F("  let res = await fetch('/' + filename);"));
  client.println(F("  let text = await res.text();"));
  client.println(F("  let lines = text.trim().split('\\n').slice(1);"));
  client.println(F("  let limit = new Date().getTime() - rangeDays * 86400000;"));
  client.println(F("  let labels=[], sensorsA=[], sensorsB=[], sensorsC=[], sensorsD=[];"));
  // Dynamic downsample rate based on selected range
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

  // Poll /threshold every 30 seconds and update chart lines if value changed
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
  client.println(F("        {label: 'Sensor A', data: tempData.sensorsA, borderColor: 'red', fill: false},"));
  client.println(F("        {label: 'Sensor B', data: tempData.sensorsB, borderColor: 'blue', fill: false},"));
  client.println(F("        {label: 'Sensor C', data: tempData.sensorsC, borderColor: 'green', fill: false},"));
  client.println(F("        {label: 'Sensor D', data: tempData.sensorsD, borderColor: 'orange', fill: false},"));
  client.println(F("        {label: 'Threshold', data: Array(tempData.labels.length).fill(threshold), borderColor: 'black', borderDash: [5,5], pointRadius: 0},"));
  client.println(F("        {label: 'High Threshold', data: Array(tempData.labels.length).fill(threshold + margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0},"));
  client.println(F("        {label: 'Low Threshold', data: Array(tempData.labels.length).fill(threshold - margin), borderColor: 'gray', borderDash: [2,2], pointRadius: 0}"));
  client.println(F("      ]"));
  client.println(F("    },"));
  client.println(F("    options: {"));
  client.println(F("      responsive: true,"));
  client.println(F("      maintainAspectRatio: false,"));
  client.println(F("      scales: { y: { ticks: { stepSize: 1.0 } } }"));
  client.println(F("    }"));
  client.println(F("  });"));

  client.println(F("  if(humidChart) humidChart.destroy();"));
  client.println(F("  humidChart = new Chart(document.getElementById('humidChart'), {"));
  client.println(F("    type: 'line',"));
  client.println(F("    data: {"));
  client.println(F("      labels: humidData.labels,"));
  client.println(F("      datasets: ["));
  client.println(F("        {label: 'Sensor A', data: humidData.sensorsA, borderColor: 'red', fill: false},"));
  client.println(F("        {label: 'Sensor B', data: humidData.sensorsB, borderColor: 'blue', fill: false},"));
  client.println(F("        {label: 'Sensor C', data: humidData.sensorsC, borderColor: 'green', fill: false},"));
  client.println(F("        {label: 'Sensor D', data: humidData.sensorsD, borderColor: 'orange', fill: false}"));
  client.println(F("      ]"));
  client.println(F("    },"));
  client.println(F("    options: {"));
  client.println(F("      responsive: true,"));
  client.println(F("      maintainAspectRatio: false"));
  client.println(F("    }"));
  client.println(F("  });"));

  client.println(F("  updateLastUpdate();"));
  client.println(F("}"));

  client.println(F("document.getElementById('tempRange').addEventListener('change', updateCharts);"));
  client.println(F("document.getElementById('humidRange').addEventListener('change', updateCharts);"));
  client.println(F("setInterval(updateCharts, 300000);"));
  client.println(F("setInterval(pollThreshold, 30000);"));
  client.println(F("updateCharts();"));

  client.println(F("function updateLastUpdate() {"));
  client.println(F("  const now = new Date();"));
  client.println(F("  const formatted = now.toLocaleString();"));
  client.println(F("  document.getElementById('lastUpdate').textContent = formatted;"));
  client.println(F("}"));

  client.println(F("</script></body></html>"));
}
