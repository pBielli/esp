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
  Serial.println("Public IP: " + ip);
  ip.trim();
  Serial.println("Public IP trimmed: " + ip);
  return ip;
}

String updateDDNS(String ip) {
  HTTPClient http;
  WiFiClient c;
  String url = String(cfg.ddns_upd_url);
  url.replace("$domain", String(cfg.ddns_domain));
  url.replace("$token", String(cfg.ddns_token));
  url.replace("$ip", ip);
  http.begin(c, url);
  int code = http.GET();
  String resp = "";
  if (code == HTTP_CODE_OK) {
    resp = http.getString();
    resp.trim();
    //se risposta contiene "KO" allora c'è stato un errore, altrimenti è andato tutto bene
    if(resp.indexOf("KO") == -1){
      logAdd(millis(), "DDNS updated: " + resp);
      Serial.println("DDNS: " + resp);
    } else {
      logAdd(millis(), "DDNS update failed: " + resp);
      Serial.println("DDNS update failed: " + resp);
    }
  }
  http.end();
  return resp;
}

void checkAndUpdateDDNS() {
  String pub = getPublicIP();
  String ddns = getDDNSIP();
  Serial.println("DDNS hostname: " + String(cfg.ddns_hostname));
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
