#ifndef DDNS_H
#define DDNS_H

#include <Arduino.h>

String getDDNSIP();
String getPublicIP();
String getPublicIP(int serverIdx);
String updateDDNS(String ip);
bool checkDDNS();
bool checkAndUpdateDDNS();
String getCachedDDNSIP();
String getCachedPublicIP();

#endif
