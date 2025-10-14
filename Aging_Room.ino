#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SD.h>
#include <Crypto.h>
#include <SHA256.h>
#include <base64.hpp>

// EEPROM address where the threshold is stored
#define EEPROM_TEMP_THRESHOLD_ADDR 0
            
// Threshold limits
#define MIN_THRESHOLD 20
#define MAX_THRESHOLD 50

// Default threshold if EEPROM value is invalid
#define DEFAULT_TEMP_THRESHOLD 20
float tempThreshold = DEFAULT_TEMP_THRESHOLD;

// Connection limiting definitions
#define MAX_GLOBAL_CONNECTIONS 8
#define MAX_PER_IP_CONNECTIONS 3
#define CONNECTION_TRACKING_SIZE 15
#define CONNECTION_TIMEOUT 300000

// --- NTP & Ethernet config ---
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x30, 0x21 };
EthernetUDP Udp;
unsigned int localPort = 5203;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

unsigned long currentEpoch = 0;
unsigned long lastNtpCheck = 0;
const unsigned long ntpInterval = 86400000;

// --- DHT22 and LCD setup ---
#define DHTTYPE DHT22
DHT dhtA(2, DHTTYPE), dhtB(3, DHTTYPE), dhtC(5, DHTTYPE), dhtD(6, DHTTYPE);

LiquidCrystal_I2C lcd(0x3F, 20, 4);
#define RED_LED_PIN 8
#define GREEN_LED_PIN 7
#define BUTTON_PIN 13

const float thresholdMargin = 3.0;
const unsigned long blinkIntervalNormal = 500;
const unsigned long blinkIntervalFast = 250;
const unsigned long sensorReadInterval = 2000;

unsigned long lastDisplaySwitch = 0;
unsigned long lastBlinkToggle = 0;
unsigned long lastSensorRead = 0;

int displayMode = 0;
bool blinkState = false;

float tA = NAN, tB = NAN, tC = NAN, tD = NAN;
float hA = NAN, hB = NAN, hC = NAN, hD = NAN;

// --- SD card ---
const int chipSelect = 4;
const unsigned long csvWriteInterval = 300000;
unsigned long lastCsvWrite = 0;

// --- Ethernet Server ---
EthernetServer server(80);

#define AUTH_USERNAME "Seegrid"
#define AUTH_PASSWORD_SHA256 "8b3d7f4a1c2e9f6b5a8d3c1e4f7a9b2c5d8e1f4a7b0c3d6e9f2a5b8c1d4e7f0a"

// Connection tracking structure
struct ConnectionTracker {
  IPAddress ip;
  uint8_t activeConnections;
  unsigned long lastConnectionTime;
};

ConnectionTracker connectionTrackers[CONNECTION_TRACKING_SIZE];
uint8_t globalConnectionCount = 0;

// Function prototypes
void initConnectionTracking();
void cleanupStaleConnections();
bool canAcceptConnection(IPAddress clientIP);
void releaseConnection(IPAddress clientIP);
void sendServiceUnavailable(EthernetClient &client);
bool checkAuth(String httpRequest);

// Initialize connection tracking
void initConnectionTracking() {
  for (int i = 0; i < CONNECTION_TRACKING_SIZE; i++) {
    connectionTrackers[i].ip = IPAddress(0, 0, 0, 0);
    connectionTrackers[i].activeConnections = 0;
    connectionTrackers[i].lastConnectionTime = 0;
  }
  globalConnectionCount = 0;
}

// Clean up stale connection records
void cleanupStaleConnections() {
  unsigned long now = millis();
  for (int i = 0; i < CONNECTION_TRACKING_SIZE; i++) {
    if (connectionTrackers[i].activeConnections > 0 && 
        (now - connectionTrackers[i].lastConnectionTime > CONNECTION_TIMEOUT)) {
      globalConnectionCount -= connectionTrackers[i].activeConnections;
      connectionTrackers[i].activeConnections = 0;
      connectionTrackers[i].ip = IPAddress(0, 0, 0, 0);
    }
  }
}

// Check if a new connection can be accepted
bool canAcceptConnection(IPAddress clientIP) {
  if (globalConnectionCount >= MAX_GLOBAL_CONNECTIONS) {
    Serial.print("Global connection limit reached: ");
    Serial.println(globalConnectionCount);
    return false;
  }

  int trackerIndex = -1;
  int emptySlot = -1;
  
  for (int i = 0; i < CONNECTION_TRACKING_SIZE; i++) {
    if (connectionTrackers[i].ip == clientIP && connectionTrackers[i].activeConnections > 0) {
      trackerIndex = i;
      break;
    }
    if (emptySlot == -1 && connectionTrackers[i].activeConnections == 0) {
      emptySlot = i;
    }
  }

  if (trackerIndex != -1) {
    if (connectionTrackers[trackerIndex].activeConnections >= MAX_PER_IP_CONNECTIONS) {
      Serial.print("Per-IP limit reached for: ");
      Serial.print(clientIP);
      Serial.print(" (");
      Serial.print(connectionTrackers[trackerIndex].activeConnections);
      Serial.println(" connections)");
      return false;
    }
    connectionTrackers[trackerIndex].activeConnections++;
    connectionTrackers[trackerIndex].lastConnectionTime = millis();
  } else if (emptySlot != -1) {
    connectionTrackers[emptySlot].ip = clientIP;
    connectionTrackers[emptySlot].activeConnections = 1;
    connectionTrackers[emptySlot].lastConnectionTime = millis();
  } else {
    Serial.println("Connection tracking array full");
    return false;
  }

  globalConnectionCount++;
  
  Serial.print("Connection accepted. IP: ");
  Serial.print(clientIP);
  Serial.print(" | Global: ");
  Serial.print(globalConnectionCount);
  Serial.print("/");
  Serial.println(MAX_GLOBAL_CONNECTIONS);
  
  return true;
}

// Release a connection
void releaseConnection(IPAddress clientIP) {
  for (int i = 0; i < CONNECTION_TRACKING_SIZE; i++) {
    if (connectionTrackers[i].ip == clientIP && connectionTrackers[i].activeConnections > 0) {
      connectionTrackers[i].activeConnections--;
      connectionTrackers[i].lastConnectionTime = millis();
      
      if (connectionTrackers[i].activeConnections == 0) {
        connectionTrackers[i].ip = IPAddress(0, 0, 0, 0);
      }
      
      if (globalConnectionCount > 0) {
        globalConnectionCount--;
      }
      
      Serial.print("Connection released. IP: ");
      Serial.print(clientIP);
      Serial.print(" | Global: ");
      Serial.print(globalConnectionCount);
      Serial.print("/");
      Serial.println(MAX_GLOBAL_CONNECTIONS);
      break;
    }
  }
}

// Send 503 Service Unavailable response
void sendServiceUnavailable(EthernetClient &client) {
  client.println("HTTP/1.1 503 Service Unavailable");
  client.println("Content-Type: text/html");
  client.println("Retry-After: 60");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html><html><head><title>Service Unavailable</title></head>");
  client.println("<body><h1>503 Service Unavailable</h1>");
  client.println("<p>Server is currently at maximum capacity. Please try again later.</p>");
  client.println("</body></html>");
}

// SHA256 authentication check function
bool checkAuth(String httpRequest) {
  int authIndex = httpRequest.indexOf("Authorization: Basic ");
  if (authIndex == -1) return false;
  
  int startIndex = authIndex + 21;
  int endIndex = httpRequest.indexOf('\r', startIndex);
  if (endIndex == -1) endIndex = httpRequest.indexOf('\n', startIndex);
  
  String encodedCredentials = httpRequest.substring(startIndex, endIndex);
  encodedCredentials.trim();
  
  // Decode Base64 credentials using decode_base64
  unsigned int decodedLength = decode_base64_length((unsigned char*)encodedCredentials.c_str());
  unsigned char decodedCredentials[decodedLength + 1];
  decode_base64((unsigned char*)encodedCredentials.c_str(), decodedCredentials);
  decodedCredentials[decodedLength] = '\0';
  
  String credentials = String((char*)decodedCredentials);
  int colonIndex = credentials.indexOf(':');
  if (colonIndex == -1) return false;
  
  String username = credentials.substring(0, colonIndex);
  String password = credentials.substring(colonIndex + 1);
  
  // Check username
  if (!username.equals(AUTH_USERNAME)) return false;
  
  // Hash the provided password and compare to stored hash
  SHA256 sha256;
  sha256.reset();
  sha256.update((const byte*)password.c_str(), password.length());
  
  byte hash[32];
  sha256.finalize(hash, 32);
  
  // Convert hash to hex string
  char hashHex[65];
  for (int i = 0; i < 32; i++) {
    sprintf(&hashHex[i * 2], "%02x", hash[i]);
  }
  hashHex[64] = '\0';
  
  // Compare with stored SHA256 hash
  return String(hashHex).equals(String(AUTH_PASSWORD_SHA256));
}

void sendNTPpacket(IPAddress &address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

bool isLeapYear(int year) {
  return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

void epochToDateTime(unsigned long epoch, int &year, int &month, int &day, int &hour, int &minute, int &second, int &weekday) {
  unsigned long days = epoch / 86400UL;
  unsigned long secondsInDay = epoch % 86400UL;

  hour = secondsInDay / 3600;
  minute = (secondsInDay % 3600) / 60;
  second = secondsInDay % 60;

  year = 1970;
  while (true) {
    int daysInYear = isLeapYear(year) ? 366 : 365;
    if (days >= daysInYear) {
      days -= daysInYear;
      year++;
    } else {
      break;
    }
  }

  int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (isLeapYear(year)) daysInMonth[1] = 29;

  month = 0;
  while (days >= daysInMonth[month]) {
    days -= daysInMonth[month];
    month++;
  }
  day = days + 1;

  unsigned long daysSince1970 = epoch / 86400UL;
  weekday = (daysSince1970 + 4) % 7;
}

bool isDST(int year, int month, int day, int weekday) {
  if (month < 3 || month > 11) return false;
  if (month > 3 && month < 11) return true;
  if (month == 3) {
    int wMarch1 = (weekday - (day - 1)) % 7;
    if (wMarch1 < 0) wMarch1 += 7;
    int secondSunday = 8 + ((7 - wMarch1) % 7);
    return day >= secondSunday;
  }
  if (month == 11) {
    int daysToNov1 = day - 1;
    int wNov1 = (weekday - daysToNov1) % 7;
    if (wNov1 < 0) wNov1 += 7;
    int firstSunday = 1 + ((7 - wNov1) % 7);
    return day < firstSunday;
  }
  return false;
}

void requestNtpTime() {
  IPAddress ntpIP(129, 6, 15, 28);
  Serial.println("Sending NTP request...");
  sendNTPpacket(ntpIP);

  unsigned long start = millis();
  while (millis() - start < 2000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("NTP response received!");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long epoch = (highWord << 16) | lowWord;

      unsigned long deviceEpoch = currentEpoch;
      long offset = (long)epoch - (long)deviceEpoch;

      Serial.print("NTP Offset (seconds): ");
      Serial.println(offset);

      epoch -= 2208988800UL;

      int year, month, day, hour, minute, second, weekday;
      epochToDateTime(epoch, year, month, day, hour, minute, second, weekday);
      bool dstActive = isDST(year, month, day, weekday);
      int timeZoneOffset = dstActive ? -4 : -5;
      currentEpoch = epoch + (timeZoneOffset * 3600UL);

      Serial.print("DST Active: ");
      Serial.println(dstActive ? "Yes (EDT)" : "No (EST)");
      Serial.print("Local Date & Time: ");
      Serial.print(month + 1);
      Serial.print("-");
      Serial.print(day);
      Serial.print("-");
      Serial.print(year);
      Serial.print(" ");
      Serial.print(hour);
      Serial.print(":");
      Serial.print(minute);
      Serial.print(":");
      Serial.println(second);
      return;
    }
  }
  Serial.println("NTP response timeout.");
}

String getDateString() {
  int year, month, day, hour, minute, second, weekday;
  epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", month + 1, day, year);
  return String(buffer);
}

String getTimeString() {
  int year, month, day, hour, minute, second, weekday;
  epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, minute, second);
  return String(buffer);
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

void serveRootPage(EthernetClient &client) {
  String lastUpdate = getDateString() + " " + getTimeString();

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
  client.println(F("  let downsampleRate = 12;"));
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

  client.print(F("const threshold = "));
  client.print(tempThreshold, 1);
  client.println(F(";"));
  client.println(F("const margin = 3.0;"));

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
  client.println(F("updateCharts();"));

  client.println(F("function updateLastUpdate() {"));
  client.println(F("  const now = new Date();"));
  client.println(F("  const formatted = now.toLocaleString();"));
  client.println(F("  document.getElementById('lastUpdate').textContent = formatted;"));
  client.println(F("}"));

  client.println(F("</script></body></html>"));
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  // Initialize connection tracking
  initConnectionTracking();

  for (int i = 0; i < 5; i++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Booting");
    for (int dot = 0; dot <= i && dot < 3; dot++) lcd.print(".");
    delay(1000);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Red/Green Stack");
  lcd.setCursor(0, 1);
  lcd.print("LED Testing");
  for (int i = 0; i < 2; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    delay(250);
    digitalWrite(RED_LED_PIN, LOW);
    delay(250);
  }
  for (int i = 0; i < 2; i++) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(250);
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(250);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Testing");
  delay(1000);
  lcd.clear();
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 20; col++) {
      lcd.setCursor(col, row);
      lcd.write(255);
      delay(125);
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Getting Ready");
  lcd.setCursor(0, 2);
  lcd.print("Standby");
  delay(10000);
  lcd.clear();

  dhtA.begin();
  dhtB.begin();
  dhtC.begin();
  dhtD.begin();

  EEPROM.get(EEPROM_TEMP_THRESHOLD_ADDR, tempThreshold);
  if (tempThreshold < MIN_THRESHOLD || tempThreshold > MAX_THRESHOLD) {
    tempThreshold = DEFAULT_TEMP_THRESHOLD;
  }

  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  Ethernet.init(10);

  Serial.println("Starting Ethernet with DHCP...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP failed. Trying static IP...");

    IPAddress ip(192, 168, 16, 70);
    IPAddress gateway(192, 168, 16, 1);
    IPAddress subnet(255, 255, 255, 0);
    IPAddress dns(192, 168, 16, 1);

    Ethernet.begin(mac, ip, dns, gateway, subnet);
    Serial.println("Static IP assigned.");
  } else {
    Serial.println("DHCP successful.");
  }

  delay(1000);
  Serial.print("Ethernet IP: ");
  Serial.println(Ethernet.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ethernet IP: ");
  lcd.setCursor(0, 2);
  lcd.print(Ethernet.localIP());
  delay(10000);
  lcd.clear();

  Udp.begin(localPort);
  requestNtpTime();
  lastNtpCheck = millis();

  if (!SD.begin(chipSelect)) {
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

  lastDisplaySwitch = millis();
  server.begin();
}

void loop() {
  unsigned long now = millis();
  
  static unsigned long lastEpochUpdate = 0;
  if (now - lastEpochUpdate >= 1000) {
    currentEpoch++;
    lastEpochUpdate = now;
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long holdStart = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      digitalWrite(RED_LED_PIN, (millis() / 250) % 2);
      digitalWrite(GREEN_LED_PIN, !((millis() / 250) % 2));
      if (millis() - holdStart >= 5000) break;
      delay(50);
    }

    if (millis() - holdStart >= 5000) {
      float oldThreshold = tempThreshold;
      unsigned long lastIncTime = 0;
      lcd.clear();

      for (int i = 0; i < 10; i++) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
        delay(250);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(250);
      }

      while (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - lastBlinkToggle >= 250) {
          blinkState = !blinkState;
          lastBlinkToggle = millis();
        }

        digitalWrite(GREEN_LED_PIN, blinkState ? HIGH : LOW);
        digitalWrite(RED_LED_PIN, LOW);

        if (millis() - lastIncTime >= 2000) {
          tempThreshold += 1.0;
          if (tempThreshold > 50.0) tempThreshold = 20.0;
          lastIncTime = millis();
        }

        if (blinkState) {
          lcd.setCursor(0, 0);
          lcd.print("Adjustment Mode   ");
        } else {
          lcd.setCursor(0, 0);
          lcd.print("                  ");
        }

        lcd.setCursor(0, 1);
        lcd.print("Adjusting...      ");
        lcd.setCursor(0, 2);
        lcd.print("New Threshold: ");
        lcd.print((int)tempThreshold);
        lcd.setCursor(0, 3);
        lcd.print("Release to exit   ");
        delay(50);
      }

      EEPROM.put(0, tempThreshold);

      for (int i = 0; i < 10; i++) {
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(250);
        digitalWrite(RED_LED_PIN, LOW);
        delay(250);
      }

      lcd.clear();
      for (int i = 0; i < 20; i++) {
        lcd.setCursor(0, 0);
        lcd.print("Threshold Updated ");
        lcd.setCursor(0, 2);
        lcd.print("Old: ");
        lcd.print((int)oldThreshold);
        lcd.setCursor(0, 3);
        lcd.print("New: ");
        lcd.print((int)tempThreshold);

        digitalWrite(RED_LED_PIN, i % 2);
        digitalWrite(GREEN_LED_PIN, i % 2);
        delay(500);
      }

      lcd.clear();
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
    }
  }

  if (now - lastSensorRead >= sensorReadInterval) {
    tA = dhtA.readTemperature();
    if (isnan(tA)) {
      delay(500);
      tA = dhtA.readTemperature();
    }
    tB = dhtB.readTemperature();
    if (isnan(tB)) {
      delay(500);
      tB = dhtB.readTemperature();
    }
    tC = dhtC.readTemperature();
    if (isnan(tC)) {
      delay(500);
      tC = dhtC.readTemperature();
    }
    tD = dhtD.readTemperature();
    if (isnan(tD)) {
      delay(500);
      tD = dhtD.readTemperature();
    }

    hA = dhtA.readHumidity();
    if (isnan(hA)) {
      delay(500);
      hA = dhtA.readHumidity();
    }
    hB = dhtB.readHumidity();
    if (isnan(hB)) {
      delay(500);
      hB = dhtB.readHumidity();
    }
    hC = dhtC.readHumidity();
    if (isnan(hC)) {
      delay(500);
      hC = dhtC.readHumidity();
    }
    hD = dhtD.readHumidity();
    if (isnan(hD)) {
      delay(500);
      hD = dhtD.readHumidity();
    }

    lastSensorRead = now;
  }

  bool tempError = isnan(tA) || isnan(tB) || isnan(tC) || isnan(tD);
  bool tempOutOfRange =
    (!isnan(tA) && abs(tA - tempThreshold) > thresholdMargin) || 
    (!isnan(tB) && abs(tB - tempThreshold) > thresholdMargin) || 
    (!isnan(tC) && abs(tC - tempThreshold) > thresholdMargin) || 
    (!isnan(tD) && abs(tD - tempThreshold) > thresholdMargin);

  unsigned long blinkInterval;

  if (tempError) {
    blinkInterval = blinkIntervalFast;
  } else if (tempOutOfRange) {
    blinkInterval = blinkIntervalNormal;
  } else {
    blinkInterval = 0;
  }

  if (blinkInterval > 0 && now - lastBlinkToggle >= blinkInterval) {
    blinkState = !blinkState;
    lastBlinkToggle = now;
  }

  if (tempError) {
    digitalWrite(RED_LED_PIN, blinkState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else if (tempOutOfRange) {
    digitalWrite(RED_LED_PIN, blinkState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  }

  if (now - lastDisplaySwitch >= 10000) {
    displayMode = !displayMode;
    lcd.clear();
    lastDisplaySwitch = now;
  }

  lcd.setCursor(0, 0);
  lcd.print("Seegrid Aging Room");

  if (displayMode == 0) {
    lcd.setCursor(0, 1);
    lcd.print("Temperature       ");
    lcd.setCursor(0, 2);
    lcd.print("A: ");
    lcd.print(isnan(tA) ? (blinkState ? "ERR  " : "     ") : (abs(tA - tempThreshold) > thresholdMargin && blinkState) ? "     " : String(tA, 1) + " C");
    lcd.setCursor(10, 2);
    lcd.print("B: ");
    lcd.print(isnan(tB) ? (blinkState ? "ERR  " : "     ") : (abs(tB - tempThreshold) > thresholdMargin && blinkState) ? "     " : String(tB, 1) + " C");
    lcd.setCursor(0, 3);
    lcd.print("C: ");
    lcd.print(isnan(tC) ? (blinkState ? "ERR  " : "     ") : (abs(tC - tempThreshold) > thresholdMargin && blinkState) ? "     " : String(tC, 1) + " C");
    lcd.setCursor(10, 3);
    lcd.print("D: ");
    lcd.print(isnan(tD) ? (blinkState ? "ERR  " : "     ") : (abs(tD - tempThreshold) > thresholdMargin && blinkState) ? "     " : String(tD, 1) + " C");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Humidity          ");
    lcd.setCursor(0, 2);
    lcd.print("A: ");
    lcd.print(isnan(hA) ? (blinkState ? "ERR  " : "     ") : String(hA, 1) + " %");
    lcd.setCursor(10, 2);
    lcd.print("B: ");
    lcd.print(isnan(hB) ? (blinkState ? "ERR  " : "     ") : String(hB, 1) + " %");
    lcd.setCursor(0, 3);
    lcd.print("C: ");
    lcd.print(isnan(hC) ? (blinkState ? "ERR  " : "     ") : String(hC, 1) + " %");
    lcd.setCursor(10, 3);
    lcd.print("D: ");
    lcd.print(isnan(hD) ? (blinkState ? "ERR  " : "     ") : String(hD, 1) + " %");
  }

  if (millis() - lastNtpCheck >= ntpInterval) {
    requestNtpTime();
    lastNtpCheck = millis();
  }

  if (millis() - lastCsvWrite >= csvWriteInterval) {
    appendCsvData();
    lastCsvWrite = millis();
  }

  // Periodic cleanup of stale connections
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
      delay(1);
      client.stop();
      return;
    }

    bool currentLineIsBlank = true;
    String httpRequest = "";
    unsigned long connectionStart = millis();
    const unsigned long requestTimeout = 5000;

    while (client.connected()) {
      if (millis() - connectionStart > requestTimeout) {
        Serial.println("Request timeout");
        break;
      }

      if (client.available()) {
        char c = client.read();
        httpRequest += c;

        if (httpRequest.length() > 512) {
          client.println("HTTP/1.1 413 Request Entity Too Large");
          client.println("Connection: close");
          client.println();
          break;
        }

        if (c == '\n' && currentLineIsBlank) {
          if (httpRequest.startsWith("GET /temp.csv")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              serveFile(client, "temp.csv", "text/csv");
            }
            break;
          } else if (httpRequest.startsWith("GET /humid.csv")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              serveFile(client, "humid.csv", "text/csv");
            }
            break;
          } else if (httpRequest.startsWith("GET /delete_temp")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              SD.remove("temp.csv");
              createCsvHeaderIfNeeded();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Temperature CSV deleted.");
            }
            break;
          } else if (httpRequest.startsWith("GET /delete_humid")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              SD.remove("humid.csv");
              createCsvHeaderIfNeeded();
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Humidity CSV deleted.");
            }
            break;
          } else if (httpRequest.startsWith("GET / ") || httpRequest.startsWith("GET / HTTP")) {
            if (!checkAuth(httpRequest)) {
              client.println("HTTP/1.1 401 Unauthorized");
              client.println("WWW-Authenticate: Basic realm=\"Aging Room\"");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Authentication required.");
            } else {
              serveRootPage(client);
            }
            break;
          } else {
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println();
            client.println("404 Not Found");
            break;
          }
        }

        if (c == '\n') {
          currentLineIsBlank = true;
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