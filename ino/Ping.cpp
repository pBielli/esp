#include "Ping.h"
#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>

String pingHost(String host) {
  IPAddress ip;
  if (!ip.fromString(host)) {
    if (!WiFi.hostByName(host.c_str(), ip)) {
      return "{\"host\":\"" + host + "\",\"success\":false,\"error\":\"DNS resolution failed\"}";
    }
  }
  bool success = Ping.ping(ip, 3);
  int avg = Ping.averageTime();
  char buf[160];
  snprintf(buf, sizeof(buf), "{\"host\":\"%s\",\"ip\":\"%s\",\"success\":%s,\"rtt_ms\":%d}",
           host.c_str(), ip.toString().c_str(), success ? "true" : "false", avg);
  return String(buf);
}
