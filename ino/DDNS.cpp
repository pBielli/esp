#include "DDNS.h"
#include "Config.h"
#include "GPIO.h"
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

String getDDNSIP() {
  IPAddress ip;
  if (WiFi.hostByName(cfg.ddns_hostname, ip)) return ip.toString();
  return "";
}

String getPublicIP() {
  HTTPClient http;
  WiFiClient c;
  http.begin(c, "http://api.ipify.org");
  int code = http.GET();
  String ip = (code == HTTP_CODE_OK) ? http.getString() : "";
  http.end();
  ip.trim();
  return ip;
}

String updateDDNS(String ip) {
  HTTPClient http;
  WiFiClient c;
  String url = "http://www.duckdns.org/update?domains=" + String(cfg.duckdns_domain) + "&token=" + String(cfg.duckdns_token) + "&ip=" + ip;
  http.begin(c, url);
  int code = http.GET();
  String resp = "";
  if (code == HTTP_CODE_OK) {
    resp = http.getString();
    resp.trim();
    logAdd(millis(), "DDNS updated: " + resp);
    Serial.println("DDNS: " + resp);
  }
  http.end();
  return resp;
}

void checkAndUpdateDDNS() {
  String ddns = getDDNSIP();
  String pub = getPublicIP();
  Serial.println("DDNS: " + ddns + " Public: " + pub);
  logAdd(millis(), "Check: DDNS=" + ddns + " Public=" + pub);

  if (ddns != "" && pub != "") {
    if (ddns != pub) {
      Serial.println("Mismatch! Updating...");
      logAdd(millis(), "DDNS mismatch detected");
      ledBlink(5);
      ledOn();
      updateDDNS(pub);
    } else {
      Serial.println("Match");
      ledOff();
    }
  }
}
