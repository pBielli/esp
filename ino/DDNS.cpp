#include "DDNS.h"
#include "Config.h"
#include "GPIO.h"
#include "Logger.h"
#include <ArduinoJson.h>
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
unsigned long lastCheckTime = 0;
bool lastCheckMatch = false;
String lastCheckedPublicIP = "";
String lastCheckedDomainIP = "";

String getCachedPublicIP() { return _cachedPublicIP; }
String getCachedDDNSIP() { return _cachedDDNSIP; }

String getPublicIP() {
  String urls = String(cfg.public_ip_urls);
  while (urls.length() > 0) {
    int comma = urls.indexOf(',');
    String host = (comma == -1) ? urls : urls.substring(0, comma);
    urls = (comma == -1) ? "" : urls.substring(comma + 1);
    host.trim();
    if (host == "") continue;
    HTTPClient http; http.setTimeout(10000); http.setUserAgent("ESP-DDNS/1.0");
    String ip;
    int code;

    WiFiClientSecure cs;
    cs.setInsecure();
    http.begin(cs, "https://" + host);
    code = http.GET();
    if (code == HTTP_CODE_OK) { ip = http.getString(); ip.trim(); }
    http.end();
    if (ip != "") return ip;

    WiFiClient c;
    http.begin(c, "http://" + host);
    code = http.GET();
    if (code == HTTP_CODE_OK) { ip = http.getString(); ip.trim(); }
    http.end();
    if (ip != "") return ip;

    logPrint("DDNS", "Public IP server failed: " + host + " (code=" + String(code) + "), trying next...");
  }
  return "";
}

String getPublicIP(int serverIdx) {
  String urls = String(cfg.public_ip_urls);
  int idx = 0;
  while (urls.length() > 0) {
    int comma = urls.indexOf(',');
    String host = (comma == -1) ? urls : urls.substring(0, comma);
    urls = (comma == -1) ? "" : urls.substring(comma + 1);
    host.trim();
    if (host == "") continue;
    if (idx == serverIdx) {
      HTTPClient http; http.setTimeout(10000); http.setUserAgent("ESP-DDNS/1.0");
      String ip;

      WiFiClientSecure cs;
      cs.setInsecure();
      http.begin(cs, "https://" + host);
      int code = http.GET();
      if (code == HTTP_CODE_OK) { ip = http.getString(); ip.trim(); }
      http.end();
      if (ip != "") return ip;

      WiFiClient c;
      http.begin(c, "http://" + host);
      code = http.GET();
      if (code == HTTP_CODE_OK) { ip = http.getString(); ip.trim(); }
      http.end();
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
  String resp;
  int code;

  if (url.startsWith("https://")) {
    WiFiClientSecure c;
    c.setInsecure();
    http.begin(c, url);
    code = http.GET();
    if (code == HTTP_CODE_OK) { resp = http.getString(); resp.trim(); }
    http.end();

    if (code != HTTP_CODE_OK) {
      String httpUrl = url; httpUrl.replace("https://", "http://");
      WiFiClient cPlain;
      http.begin(cPlain, httpUrl);
      code = http.GET();
      if (code == HTTP_CODE_OK) { resp = http.getString(); resp.trim(); }
      http.end();
    }
  } else {
    WiFiClient c;
    http.begin(c, url);
    code = http.GET();
    if (code == HTTP_CODE_OK) { resp = http.getString(); resp.trim(); }
    http.end();
  }

  if (code == HTTP_CODE_OK) {
    if(resp.indexOf("KO") == -1){
      logPrint("DDNS", "Updated: " + resp);
    } else {
      logPrint("DDNS", "Update failed: " + resp);
    }
  } else {
    resp = "KO_HTTP:" + String(code);
    logPrint("DDNS", "HTTP failed: " + resp);
  }
  http.end();
  return resp;
}

bool checkDDNS() {
  String pub = getPublicIP();
  String ddns = getDDNSIP();
  _cachedPublicIP = pub;
  _cachedDDNSIP = ddns;
  lastCheckTime = millis();
  lastCheckedPublicIP = pub;
  lastCheckedDomainIP = ddns;
  bool flag=false;
  if (ddns != "" && pub != "") {
    if (ddns != pub) {
      logPrint("DDNS", "Mismatch: DDNS=" + ddns + " Public=" + pub + " — auto-updating");
      updateDDNS(pub);
      String newDdns = getDDNSIP();
      lastCheckMatch = (newDdns == pub && newDdns != "");
      ledOff();
      flag = lastCheckMatch;
    } else {
      logPrint("DDNS", "Match OK: " + ddns);
      lastCheckMatch = true;
      ledOff();
      flag=true;
    }
  }
  return flag;
}

String ddnsCheck() {
  String pub = getPublicIP();
  String ddns = getDDNSIP();
  _cachedPublicIP = pub;
  _cachedDDNSIP = ddns;
  lastCheckTime = millis();
  lastCheckedPublicIP = pub;
  lastCheckedDomainIP = ddns;
  bool match = (ddns != "" && pub != "" && ddns == pub);
  lastCheckMatch = match;
  String urls = String(cfg.public_ip_urls);
  int comma = urls.indexOf(',');
  String firstServer = (comma == -1) ? urls : urls.substring(0, comma);
  firstServer.trim();
  DynamicJsonDocument doc(512);
  doc["public_ip"] = pub;
  doc["ddns_ip"] = ddns;
  doc["match"] = match;
  doc["server"] = firstServer;
  String r; serializeJson(doc, r);
  return r;
}
