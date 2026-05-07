#ifndef DDNS_H
#define DDNS_H

#include <Arduino.h>

String getDDNSIP();
String getPublicIP();
String updateDDNS(String ip);
void checkAndUpdateDDNS();

#endif
