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
extern ConnectionTracker connectionTrackers[CONNECTION_TRACKING_SIZE];
extern uint8_t globalConnectionCount;

// Function prototypes
void initConnectionTracking();
void cleanupStaleConnections();
bool canAcceptConnection(IPAddress clientIP);
void releaseConnection(IPAddress clientIP);
void sendServiceUnavailable(EthernetClient &client);
void sendNTPpacket(IPAddress &address);
void requestNtpTime();
bool isLeapYear(int year);
void epochToDateTime(unsigned long epoch, int &year, int &month, int &day, int &hour, int &minute, int &second, int &weekday);
bool isDST(int year, int month, int day, int weekday);
String getDateString();
String getTimeString();

#endif // NETWORK_H