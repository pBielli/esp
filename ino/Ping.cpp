#include "Ping.h"
#include <ESP8266WiFi.h>
#include <ESPping.h>

String pingHost(String host) {
  IPAddress ip;
  if (!WiFi.hostByName(host.c_str(), ip)) {
    return "{\"host\":\"" + host + "\",\"success\":false,\"error\":\"DNS resolution failed\"}";
  }
  bool success = Ping.ping(ip, 3);
  float avg = Ping.averageTime();
  char buf[160];
  snprintf(buf, sizeof(buf), "{\"host\":\"%s\",\"ip\":\"%s\",\"success\":%s,\"rtt_ms\":%.1f}",
           host.c_str(), ip.toString().c_str(), success ? "true" : "false", avg);
  return String(buf);
}
