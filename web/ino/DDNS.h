#ifndef DDNS_H
#define DDNS_H

#include <Arduino.h>

String getDDNSIP();
String getPublicIP();
String getPublicIP(int serverIdx);
String updateDDNS(String ip);
bool checkDDNS();
String getCachedDDNSIP();
String getCachedPublicIP();
String ddnsCheck();
extern unsigned long lastCheckTime;
extern bool lastCheckMatch;
extern String lastCheckedPublicIP;
extern String lastCheckedDomainIP;

#endif
