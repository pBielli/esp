#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <DNSServer.h>

extern DNSServer dnsServer;

void wifiSetup();
void wifiLoop();
bool wifiAPStart();
void wifiAPStop();
bool wifiAPRunning();
int wifiAPClientCount();
String wifiAPSSID();
String wifiAPIP();

#endif
