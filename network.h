#ifndef NETWORK_H
#define NETWORK_H

#include "config.h"
#include <Ethernet.h>
#include <EthernetUdp.h>

// Connection tracking structure
struct ConnectionTracker {
  IPAddress ip;
  uint8_t activeConnections;
  unsigned long lastConnectionTime;
};

// External variables
extern EthernetUDP Udp;
extern EthernetServer server;
extern byte mac[6];
extern byte packetBuffer[NTP_PACKET_SIZE];
extern unsigned long currentEpoch;
extern unsigned long lastNtpCheck;
extern unsigned long lastNtpEpoch;
extern ConnectionTracker connectionTrackers[CONNECTION_TRACKING_SIZE];
extern uint8_t globalConnectionCount;

// Function prototypes
void initConnectionTracking();
void cleanupStaleConnections();
bool canAcceptConnection(IPAddress clientIP);
void releaseConnection(IPAddress clientIP);
void sendServiceUnavailable(EthernetClient &client);
void sendNTPpacket(IPAddress &address);
bool tryNtpSync(IPAddress ntpIP, const char* serverName);
void requestNtpTime();
bool isLeapYear(int year);
void epochToDateTime(unsigned long epoch, int &year, int &month, int &day,
                     int &hour, int &minute, int &second, int &weekday);
int nthWeekdayOfMonth(int year, int month, int targetWeekday, int n);
bool isDST(int year, int month, int day, int hour);

// Buffer-based date/time — callers supply the buffer; no heap String allocation
void getDateString(char* buf, size_t bufLen);  // buf must be >= 11 bytes: "MM-DD-YYYY\0"
void getTimeString(char* buf, size_t bufLen);  // buf must be >=  9 bytes: "HH:MM:SS\0"

#endif // NETWORK_H
