#include "DDNS.h"
#include "Config.h"
#include "GPIO.h"
#include "Logger.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

String getDDNSIP() {
  IPAddress ip;
  if (WiFi.hostByName(cfg.ddns_hostname, ip)) return ip.toString();
  return "";
}

String _cachedPublicIP = "";
String _cachedDDNSIP = "";

String getCachedPublicIP() { return _cachedPublicIP; }
String getCachedDDNSIP() { return _cachedDDNSIP; }

String getPublicIP() {
  String urls = String(cfg.public_ip_urls);
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("ESP-DDNS/1.0");
  while (urls.length() > 0) {
    int comma = urls.indexOf(',');
    String host = (comma == -1) ? urls : urls.substring(0, comma);
    urls = (comma == -1) ? "" : urls.substring(comma + 1);
    host.trim();
    if (host == "") continue;
    String url = "https://" + host;
    WiFiClientSecure c;
    c.setInsecure();
    http.begin(c, url);
    int code = http.GET();
    String ip = (code == HTTP_CODE_OK) ? http.getString() : "";
    http.end();
    ip.trim();
    if (ip != "") {
      return ip;
    }
    Serial.println("getPublicIP: " + url + " failed (code=" + String(code) + "), trying next...");
    logAdd(millis(), "Public IP server failed: " + url);
  }
  return "";
}

String getPublicIP(int serverIdx) {
  String urls = String(cfg.public_ip_urls);
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("ESP-DDNS/1.0");
  int idx = 0;
  while (urls.length() > 0) {
    int comma = urls.indexOf(',');
    String host = (comma == -1) ? urls : urls.substring(0, comma);
    urls = (comma == -1) ? "" : urls.substring(comma + 1);
    host.trim();
    if (host == "") continue;
    if (idx == serverIdx) {
      String url = "https://" + host;
      WiFiClientSecure c;
      c.setInsecure();
      http.begin(c, url);
      int code = http.GET();
      String ip = (code == HTTP_CODE_OK) ? http.getString() : "";
      http.end();
      ip.trim();
      return ip;
    }
    idx++;
  }
  return "";
}

String updateDDNS(String ip) {
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("ESP-DDNS/1.0");
  String url = String(cfg.ddns_upd_url);
  url.replace("$domain", String(cfg.ddns_domain));
  url.replace("$token", String(cfg.ddns_token));
  url.replace("$ip", ip);
  int code;
  if (url.startsWith("https://")) {
    WiFiClientSecure c;
    c.setInsecure();
    http.begin(c, url);
    code = http.GET();
  } else {
    WiFiClient c;
    http.begin(c, url);
    code = http.GET();
  }
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

bool checkDDNS() {
  String pub = getPublicIP();
  String ddns = getDDNSIP();
  _cachedPublicIP = pub;
  _cachedDDNSIP = ddns;
  bool flag=false;
  if (ddns != "" && pub != "") {
    if (ddns != pub) {
      Serial.println("Mismatch! Updating...");
      logAdd(millis(), "DDNS mismatch detected");
      ledBlink(5);
      ledOn();
    } else {
      Serial.println("Match");
      ledOff();
      flag=true;
    }
  }
  Serial.println("Check DDNS:"+ String(cfg.ddns_hostname) + " - DDNS=" + ddns + " Public=" + pub + "Match=" + (flag?"True":"False"));
  logAdd(millis(), "Check DDNS:"+ String(cfg.ddns_hostname) + " - DDNS=" + ddns + " Public=" + pub + "Match=" + (flag?"True":"False"));
  return flag;
}
bool checkAndUpdateDDNS() {
  bool flag = checkDDNS();
  if (!flag) {
    String resp = updateDDNS(_cachedPublicIP);
    flag=resp.indexOf("KO") == -1;
    Serial.println("Update DDNS:"+ String(cfg.ddns_hostname) + " - Match:" + (flag ? "True" : "False")+" - Resp: " + resp );
    logAdd(millis(), "Update DDNS:"+ String(cfg.ddns_hostname) + " - Match:" + (flag ? "True" : "False")+" - Resp: " + resp );
  }
  return flag;
}
