#include "WebServer.h"
#include "Config.h"
#include "Logger.h"
#include "NTP.h"
#include "GPIO.h"
#include "DDNS.h"
#include "Ping.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "WebServer_index.h"
ESP8266WebServer server(80);

String base64Decode(String input) {
  const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int len = input.length();
  String out = "";
  int buf = 0, bits = 0;
  for (int i = 0; i < len; i++) {
    char c = input[i];
    if (c == '=') break;
    const char* p = strchr(b64, c);
    if (!p) continue;
    int val = p - b64;
    buf = (buf << 6) | val;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += char((buf >> bits) & 0xFF);
    }
  }
  return out;
}

void sendCORS();

bool checkAuth() {
  sendCORS();
  if (!server.hasHeader("Authorization")) {
    server.sendHeader("WWW-Authenticate", "Basic realm=\"Login\"");
    server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return false;
  }
  String auth = server.header("Authorization");
  if (!auth.startsWith("Basic ")) return false;
  auth = base64Decode(auth.substring(6));
  return auth == "admin:" + String(cfg.admin_password);
}

void sendCORS() {
  server.sendHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
}

void setupRoutes() {
  // Catch-all OPTIONS handler for undefined routes
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      sendCORS();
      server.send(200);
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  // Helper lambda for OPTIONS
  auto addOptions = [&](const char* path) {
    server.on(path, HTTP_OPTIONS, [path]() {
      sendCORS();
      server.send(200);
    });
  };

  addOptions("/");
  server.on("/", []() {
    sendCORS();
    server.send(200, "text/html", index_page);
  });


  addOptions("/api/help");
  server.on("/api/help", []() {
    sendCORS();
    JsonDocument doc;
    JsonArray a = doc["endpoints"].to<JsonArray>();
    a.add("/api/info");
    a.add("/api/myip");
    a.add("/api/help");
    a.add("/api/time");
    a.add("/api/log");
    a.add("/api/blink?times=N");
    a.add("/api/ddns/update");
    a.add("/api/ddns/config (POST: hostname,domain,token,upd_url)");
    a.add("/api/resolve?host=x");
    a.add("/api/curl?url=x");
    a.add("/api/gpio/info");
    a.add("/api/gpio/read?pin=X");
    a.add("/api/gpio/analog/read");
    a.add("/api/gpio/analog/write (POST: pin,value)");
    a.add("/api/gpio/set (POST: pin,mode,value)");
    a.add("/api/led/pin (POST)");
    a.add("/api/led/invert (POST: enabled)");
    a.add("/api/led/on");
    a.add("/api/led/off");
    a.add("/api/led/blink");
    a.add("/api/ntp/set (POST: server,tz_offset)");
    a.add("/api/wifi/scan");
    a.add("/api/set/mdns (POST)");
    a.add("/api/dns/set (POST: dns1,dns2)");
    a.add("/api/ip/config (POST: dhcp,ip,gateway,subnet)");
    a.add("/api/pwm/pin (POST)");
    a.add("/api/set/ssid (POST)");
    a.add("/api/set/pswd (POST)");
    a.add("/api/ping?host=x");
    a.add("/api/system/reboot (POST)");
    a.add("/api/system/factory-reset (POST)");
    a.add("/api/system/version");
    a.add("/api/config/export");
    a.add("/api/config/import (POST)");
    a.add("/api/ddns/interval (POST)");
    a.add("/api/ddns/ipurls (GET, POST: urls)");
    a.add("/api/gpio/pulse (POST: pin,ms)");
    a.add("/api/gpio/toggle (POST: pin)");
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/info");
  server.on("/api/info", []() {
    sendCORS();
    JsonDocument doc;
    doc["mac"] = WiFi.macAddress();
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["mdns"] = cfg.mdns_name;
    doc["ddns"] = cfg.ddns_hostname;
    doc["ddns_domain"] = cfg.ddns_domain;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["local_ip"] = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"] = WiFi.subnetMask().toString();
    doc["dns1"] = WiFi.dnsIP(0).toString();
    doc["dns2"] = WiFi.dnsIP(1).toString();
    doc["ntp_server"] = cfg.ntp_server;
    doc["tz_offset"] = cfg.tz_offset;
    doc["led_invert"] = cfg.led_invert;
    doc["led_pin"] = cfg.led_pin;
    doc["use_static_ip"] = cfg.use_static_ip;
    doc["static_ip"] = cfg.static_ip;
    doc["static_gateway"] = cfg.static_gateway;
    doc["static_subnet"] = cfg.static_subnet;
    doc["static_dns1"] = cfg.static_dns1;
    doc["static_dns2"] = cfg.static_dns2;
    doc["pwm_pin"] = cfg.pwm_pin;
    doc["use_custom_dns"] = cfg.use_custom_dns;
    doc["ddns_check_interval"] = cfg.ddns_check_interval;
    doc["ddns_upd_url"] = cfg.ddns_upd_url;
    doc["public_ip_urls"] = cfg.public_ip_urls;
    doc["firmware"] = FIRMWARE_VERSION;
    String ddnsIp = getCachedDDNSIP();
    if (ddnsIp != "") doc["ddns_ip"] = ddnsIp;
    String pubIp = getCachedPublicIP();
    if (pubIp != "") doc["public_ip"] = pubIp;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/myip");
  server.on("/api/myip", []() {
    sendCORS();
    String pub = getPublicIP();
    if (pub != "") {
      server.send(200, "application/json", "{\"public_ip\":\"" + pub + "\"}");
    } else {
      server.send(500, "application/json", "{\"error\":\"Unable to get public IP\"}");
    }
  });

  addOptions("/api/time");
  server.on("/api/time", []() {
    sendCORS();
    time_t epoch = ntpGetEpoch();
    int tz_hours = cfg.tz_offset / 3600;
    char utc_str[16];
    snprintf(utc_str, sizeof(utc_str), "UTC%+dh", tz_hours);
    JsonDocument doc;
    doc["time"] = ntpGetTime();
    doc["epoch"] = (long)epoch;
    doc["tz_offset"] = cfg.tz_offset;
    doc["utc"] = utc_str;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/log");
  server.on("/api/log", []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", logGet());
  });

  addOptions("/api/blink");
  server.on("/api/blink", []() {
    if (!checkAuth()) return;
    int n = server.arg("times").toInt(); if (n < 1 || n > 100) n = 5;
    ledBlink(n);
    server.send(200, "application/json", "{\"blinked\":" + String(n) + "}");
  });

  addOptions("/api/ddns/update");
  server.on("/api/ddns/update", []() {
    if (!checkAuth()) return;
    String ip = getPublicIP();
    String resp = updateDDNS(ip);
    server.send(200, "application/json", "{\"status\":\"updated\",\"response\":\"" + resp + "\"}");
  });

  addOptions("/api/ddns/config");
  server.on("/api/ddns/config", HTTP_POST, []() {
    if (!checkAuth()) return;
    String hostname = server.arg("hostname");
    String domain = server.arg("domain");
    String token = server.arg("token");
    String upd_url = server.arg("upd_url");
    if (hostname != "") strncpy(cfg.ddns_hostname, hostname.c_str(), sizeof(cfg.ddns_hostname) - 1);
    if (domain != "") strncpy(cfg.ddns_domain, domain.c_str(), sizeof(cfg.ddns_domain) - 1);
    if (token != "") strncpy(cfg.ddns_token, token.c_str(), sizeof(cfg.ddns_token) - 1);
    if (upd_url != "") strncpy(cfg.ddns_upd_url, upd_url.c_str(), sizeof(cfg.ddns_upd_url) - 1);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"ddns_hostname\":\"" + String(cfg.ddns_hostname) + "\",\"ddns_domain\":\"" + String(cfg.ddns_domain) + "\"}");
  });

  addOptions("/api/ddns/ipurls");
  server.on("/api/ddns/ipurls", HTTP_GET, []() {
    sendCORS();
    server.send(200, "application/json", "{\"public_ip_urls\":\"" + String(cfg.public_ip_urls) + "\"}");
  });
  server.on("/api/ddns/ipurls", HTTP_POST, []() {
    if (!checkAuth()) return;
    String urls = server.arg("urls");
    if (urls != "") strncpy(cfg.public_ip_urls, urls.c_str(), sizeof(cfg.public_ip_urls) - 1);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"public_ip_urls\":\"" + String(cfg.public_ip_urls) + "\"}");
  });

  addOptions("/api/resolve");
  server.on("/api/resolve", []() {
    sendCORS();
    String host = server.arg("host");
    if (host == "") { server.send(400, "application/json", "{\"error\":\"Missing host\"}"); return; }
    IPAddress ip;
    if (WiFi.hostByName(host.c_str(), ip)) {
      server.send(200, "application/json", "{\"host\":\"" + host + "\",\"ip\":\"" + ip.toString() + "\"}");
    } else {
      server.send(500, "application/json", "{\"error\":\"DNS failed\"}");
    }
  });

  addOptions("/api/curl");
  server.on("/api/curl", []() {
    if (!checkAuth()) return;
    String url = server.arg("url");
    if (url == "") { server.send(400, "application/json", "{\"error\":\"Missing url\"}"); return; }
    HTTPClient http; WiFiClient c;
    http.begin(c, url); int code = http.GET();
    String payload = (code == HTTP_CODE_OK) ? http.getString() : "Error: " + String(code);
    http.end();
    server.send(200, "text/plain", payload);
  });

  addOptions("/api/gpio/info");
  server.on("/api/gpio/info", []() {
    sendCORS();
    server.send(200, "application/json", gpioInfo());
  });

  addOptions("/api/gpio/read");
  server.on("/api/gpio/read", []() {
    sendCORS();
    int pin = server.arg("pin").toInt();
    server.send(200, "application/json", gpioRead(pin));
  });

  addOptions("/api/gpio/analog/read");
  server.on("/api/gpio/analog/read", []() {
    sendCORS();
    server.send(200, "application/json", analogReadPin());
  });

  addOptions("/api/gpio/analog/write");
  server.on("/api/gpio/analog/write", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    int value = server.arg("value").toInt();
    server.send(200, "application/json", analogWritePin(pin, value));
  });

  addOptions("/api/gpio/set");
  server.on("/api/gpio/set", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    String mode = server.arg("mode");
    String value = server.arg("value");
    server.send(200, "application/json", gpioSet(pin, mode, value));
  });

  addOptions("/api/led/pin");
  server.on("/api/led/pin", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    if (pin < 0 || pin > 16) { server.send(400, "application/json", "{\"error\":\"Invalid pin\"}"); return; }
    cfg.led_pin = pin;
    pinMode(cfg.led_pin, OUTPUT);
    digitalWrite(cfg.led_pin, cfg.led_invert ? LOW : HIGH);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"led_pin\":" + String(cfg.led_pin) + "}");
  });

  addOptions("/api/led/invert");
  server.on("/api/led/invert", HTTP_POST, []() {
    if (!checkAuth()) return;
    String enabled = server.arg("enabled");
    cfg.led_invert = (enabled == "1" || enabled == "true") ? 1 : 0;
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"led_invert\":" + String(cfg.led_invert) + "}");
  });

  addOptions("/api/led/on");
  server.on("/api/led/on", []() {
    if (!checkAuth()) return;
    ledOn();
    JsonDocument doc;
    doc["led_pin"] = cfg.led_pin;
    doc["state"] = "on";
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/led/off");
  server.on("/api/led/off", []() {
    if (!checkAuth()) return;
    ledOff();
    JsonDocument doc;
    doc["led_pin"] = cfg.led_pin;
    doc["state"] = "off";
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/led/blink");
  server.on("/api/led/blink", []() {
    if (!checkAuth()) return;
    int n = server.arg("times").toInt(); if (n < 1 || n > 100) n = 5;
    ledBlink(n);
    server.send(200, "application/json", "{\"blinked\":" + String(n) + "}");
  });

  addOptions("/api/set/ssid");
  server.on("/api/set/ssid", HTTP_POST, []() {
    if (!checkAuth()) return;
    String s = server.arg("ssid");
    if (s == "") { server.send(400, "application/json", "{\"error\":\"Missing ssid\"}"); return; }
    strncpy(cfg.wifi_ssid, s.c_str(), sizeof(cfg.wifi_ssid) - 1);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  });

  addOptions("/api/set/pswd");
  server.on("/api/set/pswd", HTTP_POST, []() {
    if (!checkAuth()) return;
    String p = server.arg("pswd");
    if (p == "") { server.send(400, "application/json", "{\"error\":\"Missing pswd\"}"); return; }
    strncpy(cfg.wifi_password, p.c_str(), sizeof(cfg.wifi_password) - 1);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  });

  addOptions("/api/set/mdns");
  server.on("/api/set/mdns", HTTP_POST, []() {
    if (!checkAuth()) return;
    String m = server.arg("mdns");
    if (m == "") { server.send(400, "application/json", "{\"error\":\"Missing mdns\"}"); return; }
    strncpy(cfg.mdns_name, m.c_str(), sizeof(cfg.mdns_name) - 1);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"mdns\":\"" + String(cfg.mdns_name) + "\"}");
  });

  addOptions("/api/ntp/set");
  server.on("/api/ntp/set", HTTP_POST, []() {
    if (!checkAuth()) return;
    String server_name = server.arg("server");
    String tz = server.arg("tz_offset");
    if (server_name != "") strncpy(cfg.ntp_server, server_name.c_str(), sizeof(cfg.ntp_server) - 1);
    if (tz != "") cfg.tz_offset = tz.toInt();
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"ntp_server\":\"" + String(cfg.ntp_server) + "\",\"tz_offset\":" + String(cfg.tz_offset) + "}");
  });

  addOptions("/api/wifi/scan");
  server.on("/api/wifi/scan", []() {
    if (!checkAuth()) return;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      server.send(200, "application/json", "{\"status\":\"scanning\"}");
    } else if (n == WIFI_SCAN_RUNNING) {
      server.send(200, "application/json", "{\"status\":\"scanning\"}");
    } else {
      JsonDocument doc;
      JsonArray networks = doc["networks"].to<JsonArray>();
      for (int i = 0; i < n; i++) {
        JsonObject net = networks.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["channel"] = WiFi.channel(i);
        net["encryption"] = WiFi.encryptionType(i);
      }
      String r; serializeJson(doc, r);
      WiFi.scanDelete();
      server.send(200, "application/json", r);
    }
  });

  addOptions("/api/dns/set");
  server.on("/api/dns/set", HTTP_POST, []() {
    if (!checkAuth()) return;
    String dns1 = server.arg("dns1");
    String dns2 = server.arg("dns2");
    String ucd = server.arg("use_custom_dns");
    if (dns1 != "") strncpy(cfg.static_dns1, dns1.c_str(), sizeof(cfg.static_dns1) - 1);
    if (dns2 != "") strncpy(cfg.static_dns2, dns2.c_str(), sizeof(cfg.static_dns2) - 1);
    if (ucd != "") cfg.use_custom_dns = (ucd == "1" || ucd == "true") ? 1 : 0;
    storageSave();
    if (cfg.use_custom_dns) {
      applyNetworkConfig();
    } else {
      WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask());
    }
    bool valid = validateNetworkConfig();
    JsonDocument doc;
    doc["status"] = "saved";
    doc["dns1"] = cfg.static_dns1;
    doc["dns2"] = cfg.static_dns2;
    doc["use_custom_dns"] = cfg.use_custom_dns;
    if (!valid) doc["warning"] = "config invalid, switched to DHCP";
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/ip/config");
  server.on("/api/ip/config", HTTP_POST, []() {
    if (!checkAuth()) return;
    String dhcp = server.arg("dhcp");
    cfg.use_static_ip = (dhcp == "0" || dhcp == "false") ? 1 : 0;
    if (cfg.use_static_ip) {
      String ip = server.arg("ip");
      String gw = server.arg("gateway");
      String subnet = server.arg("subnet");
      if (ip != "") strncpy(cfg.static_ip, ip.c_str(), sizeof(cfg.static_ip) - 1);
      if (gw != "") strncpy(cfg.static_gateway, gw.c_str(), sizeof(cfg.static_gateway) - 1);
      if (subnet != "") strncpy(cfg.static_subnet, subnet.c_str(), sizeof(cfg.static_subnet) - 1);
    }
    storageSave();
    bool valid = validateNetworkConfig();
    JsonDocument doc;
    doc["status"] = "saved";
    doc["use_static_ip"] = cfg.use_static_ip;
    if (!valid) doc["warning"] = "config invalid, switched to DHCP";
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/pwm/pin");
  server.on("/api/pwm/pin", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    if (pin < 0 || pin > 16) { server.send(400, "application/json", "{\"error\":\"Invalid pin\"}"); return; }
    cfg.pwm_pin = pin;
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"pwm_pin\":" + String(cfg.pwm_pin) + "}");
  });

  addOptions("/api/ping");
  server.on("/api/ping", []() {
    sendCORS();
    String host = server.arg("host");
    if (host == "") { server.send(400, "application/json", "{\"error\":\"Missing host\"}"); return; }
    String result = pingHost(host);
    server.send(200, "application/json", result);
  });

  addOptions("/api/system/reboot");
  server.on("/api/system/reboot", HTTP_POST, []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", "{\"status\":\"rebooting\"}");
    delay(500);
    ESP.restart();
  });

  addOptions("/api/system/factory-reset");
  server.on("/api/system/factory-reset", HTTP_POST, []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", "{\"status\":\"factory_reset\"}");
    delay(500);
    storageReset();
    ESP.restart();
  });

  addOptions("/api/system/version");
  server.on("/api/system/version", []() {
    sendCORS();
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = __DATE__;
    doc["build_time"] = __TIME__;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/config/export");
  server.on("/api/config/export", []() {
    if (!checkAuth()) return;
    JsonDocument doc;
    doc["wifi_ssid"] = cfg.wifi_ssid;
    doc["wifi_password"] = cfg.wifi_password;
    doc["mdns_name"] = cfg.mdns_name;
    doc["admin_password"] = cfg.admin_password;
    doc["ddns_hostname"] = cfg.ddns_hostname;
    doc["ddns_domain"] = cfg.ddns_domain;
    doc["ddns_token"] = cfg.ddns_token;
    doc["led_pin"] = cfg.led_pin;
    doc["tz_offset"] = cfg.tz_offset;
    doc["ntp_server"] = cfg.ntp_server;
    doc["led_invert"] = cfg.led_invert;
    doc["use_static_ip"] = cfg.use_static_ip;
    doc["static_ip"] = cfg.static_ip;
    doc["static_gateway"] = cfg.static_gateway;
    doc["static_subnet"] = cfg.static_subnet;
    doc["static_dns1"] = cfg.static_dns1;
    doc["static_dns2"] = cfg.static_dns2;
    doc["pwm_pin"] = cfg.pwm_pin;
    doc["use_custom_dns"] = cfg.use_custom_dns;
    doc["ddns_check_interval"] = cfg.ddns_check_interval;
    doc["ddns_upd_url"] = cfg.ddns_upd_url;
    doc["public_ip_urls"] = cfg.public_ip_urls;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/config/import");
  server.on("/api/config/import", HTTP_POST, []() {
    if (!checkAuth()) return;
    String body = server.arg("config");
    if (body == "") { server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}"); return; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) { server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    auto setStr = [&](const char* key, char* dest, size_t sz) {
      if (doc[key].is<const char*>()) strncpy(dest, doc[key], sz - 1);
    };
    auto setInt = [&](const char* key, int& dest) {
      if (doc[key].is<int>()) dest = doc[key].as<int>();
    };
    setStr("wifi_ssid", cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
    setStr("wifi_password", cfg.wifi_password, sizeof(cfg.wifi_password));
    setStr("mdns_name", cfg.mdns_name, sizeof(cfg.mdns_name));
    setStr("admin_password", cfg.admin_password, sizeof(cfg.admin_password));
    setStr("ddns_hostname", cfg.ddns_hostname, sizeof(cfg.ddns_hostname));
    setStr("ddns_domain", cfg.ddns_domain, sizeof(cfg.ddns_domain));
    setStr("ddns_token", cfg.ddns_token, sizeof(cfg.ddns_token));
    setInt("led_pin", cfg.led_pin);
    setInt("tz_offset", cfg.tz_offset);
    setStr("ntp_server", cfg.ntp_server, sizeof(cfg.ntp_server));
    setInt("led_invert", cfg.led_invert);
    setInt("use_static_ip", cfg.use_static_ip);
    setStr("static_ip", cfg.static_ip, sizeof(cfg.static_ip));
    setStr("static_gateway", cfg.static_gateway, sizeof(cfg.static_gateway));
    setStr("static_subnet", cfg.static_subnet, sizeof(cfg.static_subnet));
    setStr("static_dns1", cfg.static_dns1, sizeof(cfg.static_dns1));
    setStr("static_dns2", cfg.static_dns2, sizeof(cfg.static_dns2));
    setInt("pwm_pin", cfg.pwm_pin);
    setInt("use_custom_dns", cfg.use_custom_dns);
    setInt("ddns_check_interval", cfg.ddns_check_interval);
    setStr("ddns_upd_url", cfg.ddns_upd_url, sizeof(cfg.ddns_upd_url));
    setStr("public_ip_urls", cfg.public_ip_urls, sizeof(cfg.public_ip_urls));
    storageSave();
    server.send(200, "application/json", "{\"status\":\"imported\"}");
  });

  addOptions("/api/ddns/interval");
  server.on("/api/ddns/interval", HTTP_POST, []() {
    if (!checkAuth()) return;
    int sec = server.arg("interval").toInt();
    if (sec < 10 || sec > 86400) { server.send(400, "application/json", "{\"error\":\"Interval must be 10-86400 seconds\"}"); return; }
    cfg.ddns_check_interval = sec;
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"ddns_check_interval\":" + String(cfg.ddns_check_interval) + "}");
  });

  addOptions("/api/gpio/pulse");
  server.on("/api/gpio/pulse", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    int ms = server.arg("ms").toInt();
    if (ms < 1 || ms > 10000) ms = 500;
    server.send(200, "application/json", gpioPulse(pin, ms));
  });

  addOptions("/api/gpio/toggle");
  server.on("/api/gpio/toggle", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    server.send(200, "application/json", gpioToggle(pin));
  });

  server.begin();
  Serial.println("HTTP server started");
}
