#include "network.h"
#include <avr/wdt.h>

// Global variables
byte mac[] = { 0xA8, 0x61, 0xDA, 0xAE, 0xE1, 0x24 };
EthernetUDP Udp;
EthernetServer server(SERVER_PORT);
byte packetBuffer[NTP_PACKET_SIZE];
unsigned long currentEpoch = 0;
bool lastNtpCheck = false;
unsigned long lastNtpEpoch = 0;
ConnectionTracker connectionTrackers[CONNECTION_TRACKING_SIZE];
uint8_t globalConnectionCount = 0;

void initConnectionTracking() {
  for (int i = 0; i < CONNECTION_TRACKING_SIZE; i++) {
    connectionTrackers[i].ip = IPAddress(0, 0, 0, 0);
    connectionTrackers[i].activeConnections = 0;
    connectionTrackers[i].lastConnectionTime = 0;
  }
  globalConnectionCount = 0;
}

void cleanupStaleConnections() {
  unsigned long now = millis();
  for (int i = 0; i < CONNECTION_TRACKING_SIZE; i++) {
    if (connectionTrackers[i].activeConnections > 0 &&
        (now - connectionTrackers[i].lastConnectionTime > CONNECTION_TIMEOUT)) {
      // Guard against globalConnectionCount underflow before subtracting
      if (globalConnectionCount >= connectionTrackers[i].activeConnections) {
        globalConnectionCount -= connectionTrackers[i].activeConnections;
      } else {
        globalConnectionCount = 0;
      }
      connectionTrackers[i].activeConnections = 0;
      connectionTrackers[i].ip = IPAddress(0, 0, 0, 0);
    }
  }
}

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
  return true;
}

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
      break;
    }
  }
}

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

void epochToDateTime(unsigned long epoch, int &year, int &month, int &day,
                     int &hour, int &minute, int &second, int &weekday) {
  unsigned long days = epoch / 86400UL;
  unsigned long secondsInDay = epoch % 86400UL;

  hour   = secondsInDay / 3600;
  minute = (secondsInDay % 3600) / 60;
  second = secondsInDay % 60;

  year = 1970;
  while (true) {
    int daysInYear = isLeapYear(year) ? 366 : 365;
    if (days >= (unsigned long)daysInYear) {
      days -= daysInYear;
      year++;
    } else {
      break;
    }
  }

  int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (isLeapYear(year)) daysInMonth[1] = 29;

  month = 0;
  while (days >= (unsigned long)daysInMonth[month]) {
    days -= daysInMonth[month];
    month++;
  }
  day = days + 1;

  // weekday: 0=Sunday, 1=Monday, ..., 6=Saturday
  // January 1, 1970 was a Thursday = 4
  unsigned long daysSince1970 = epoch / 86400UL;
  weekday = (daysSince1970 + 4) % 7;
}

int nthWeekdayOfMonth(int year, int month, int targetWeekday, int n) {
  static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  int y = year;
  if (month < 3) y--;
  int wdayOf1st = (y + y/4 - y/100 + y/400 + t[month-1] + 1) % 7;

  int daysUntil = (targetWeekday - wdayOf1st + 7) % 7;
  int firstOccurrence = 1 + daysUntil;
  return firstOccurrence + (n - 1) * 7;
}

bool isDST(int year, int month, int day, int hour) {
  if (month < 3 || month > 11) return false;
  if (month > 3 && month < 11) return true;

  if (month == 3) {
    int dstStart = nthWeekdayOfMonth(year, 3, 0, 2);
    if (day > dstStart) return true;
    if (day < dstStart) return false;
    return hour >= 2;
  }

  if (month == 11) {
    int dstEnd = nthWeekdayOfMonth(year, 11, 0, 1);
    if (day < dstEnd) return true;
    if (day > dstEnd) return false;
    return hour < 2;
  }

  return false;
}

bool tryNtpSync(IPAddress ntpIP, const char* serverName) {
  Serial.print("Sending NTP request to ");
  Serial.print(serverName);
  Serial.print(" (");
  Serial.print(ntpIP);
  Serial.println(")...");

  sendNTPpacket(ntpIP);

  unsigned long start = millis();
  while (millis() - start < 3000) {
    wdt_reset();
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("NTP response received!");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
      unsigned long epoch    = (highWord << 16) | lowWord;
      epoch -= 2208988800UL;

      int year, month, day, hour, minute, second, weekday;
      epochToDateTime(epoch, year, month, day, hour, minute, second, weekday);

      bool dstActive = isDST(year, month + 1, day, hour);
      int timeZoneOffset = dstActive ? -4 : -5;
      currentEpoch = epoch + (timeZoneOffset * 3600UL);
      lastNtpEpoch = currentEpoch;

      int lyear, lmonth, lday, lhour, lminute, lsecond, lweekday;
      epochToDateTime(currentEpoch, lyear, lmonth, lday, lhour, lminute, lsecond, lweekday);

      Serial.print("DST Active: ");
      Serial.println(dstActive ? "Yes (EDT)" : "No (EST)");
      Serial.print("Local Date & Time: ");
      Serial.print(lmonth + 1); Serial.print("-");
      Serial.print(lday);       Serial.print("-");
      Serial.print(lyear);      Serial.print(" ");
      Serial.print(lhour);      Serial.print(":");
      if (lminute < 10) Serial.print("0");
      Serial.print(lminute);    Serial.print(":");
      if (lsecond < 10) Serial.print("0");
      Serial.println(lsecond);
      return true;
    }
    delay(10);
  }

  Serial.print("NTP timeout from ");
  Serial.println(serverName);
  return false;
}

void requestNtpTime() {
  IPAddress primaryNTP(192, 168, 80, 8);
  if (tryNtpSync(primaryNTP, "192.168.80.8")) return;

  IPAddress fallbackNTP(216, 239, 35, 0);
  if (tryNtpSync(fallbackNTP, "pool.ntp.org")) return;

  Serial.println("NTP sync failed on all servers.");
}

// Writes date as MM-DD-YYYY into caller-supplied buffer (must be >= 11 bytes)
void getDateString(char* buf, size_t bufLen) {
  int year, month, day, hour, minute, second, weekday;
  epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);
  snprintf(buf, bufLen, "%02d-%02d-%04d", month + 1, day, year);
}

// Writes time as HH:MM:SS into caller-supplied buffer (must be >= 9 bytes)
void getTimeString(char* buf, size_t bufLen) {
  int year, month, day, hour, minute, second, weekday;
  epochToDateTime(currentEpoch, year, month, day, hour, minute, second, weekday);
  snprintf(buf, bufLen, "%02d:%02d:%02d", hour, minute, second);
}
