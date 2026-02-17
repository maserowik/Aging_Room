#include "network.h"

// Global variables
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x31, 0x24 };
EthernetUDP Udp;
EthernetServer server(SERVER_PORT);
byte packetBuffer[NTP_PACKET_SIZE];
unsigned long currentEpoch = 0;
unsigned long lastNtpCheck = 0;
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
      globalConnectionCount -= connectionTrackers[i].activeConnections;
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
  
  Serial.print("Connection accepted. IP: ");
  Serial.print(clientIP);
  Serial.print(" | Global: ");
  Serial.print(globalConnectionCount);
  Serial.print("/");
  Serial.println(MAX_GLOBAL_CONNECTIONS);
  
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
  // IPAddress ntpIP(129, 6, 15, 28);
  IPAddress ntpIP(192, 168, 80, 8);
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