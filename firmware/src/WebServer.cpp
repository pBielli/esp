#include "WebServer.h"
#include "Config.h"
#include "LED.h"
#include "Logger.h"
#include "NTP.h"
#include "GPIO.h"
#include "DDNS.h"
#include "PingUtil.h"
#include "WiFiManager.h"
#include "OTA.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "WebServer_index.h"
ESP8266WebServer server(80);

String base64Decode(String input) {
  const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int len = input.length();
  String out = "";
  int buf = 0, bits = 0;
  for (int i = 0; i < len; i++) {
    char c = input[i];
    if (c == '=') { bits = 0; continue; }
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
  if (!auth.startsWith("Basic ")) {
    server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return false;
  }
  auth = base64Decode(auth.substring(6));
  if (auth != "admin:" + String(cfg.admin_password)) {
    server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return false;
  }
  return true;
}

void sendCORS() {
  ledWebPulse();
  server.sendHeader("Access-Control-Allow-Origin", CORS_ORIGIN);
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
}

void setupRoutes() {
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      sendCORS();
      server.send(200);
      return;
    }
    if (wifiAPRunning()) {
      server.send(200, "text/html", index_page);
      return;
    }
    server.send(404, "text/plain", "Not found");
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
    DynamicJsonDocument doc(2048);
    JsonArray a = doc["endpoints"].to<JsonArray>();
    auto add = [&](const char* path, const char* method, bool auth, std::initializer_list<const char*> params) {
      JsonObject o = a.createNestedObject();
      o["path"] = path;
      o["method"] = method;
      o["auth"] = auth;
      JsonArray p = o["params"].to<JsonArray>();
      for (auto& par : params) p.add(par);
    };
    // ── New REST API ──
    add("/api/status",              "GET",  false, {});
    add("/api/help",                "GET",  false, {});
    add("/api/system/status",       "GET",  false, {});
    add("/api/system/log",          "GET",  true,  {});
    add("/api/system/reboot",       "POST", true,  {});
    add("/api/system/factory-reset","POST", true,  {});
    add("/api/network/status",      "GET",  false, {});
    add("/api/network/wifi/scan",   "GET",  true,  {});
    add("/api/network/wifi/ssid",   "POST", true,  {"ssid"});
    add("/api/network/wifi/password","POST", true,  {"password"});
    add("/api/network/ip/config",   "GET",  true,  {});
    add("/api/network/ip/config",   "POST", true,  {"dhcp","ip","gateway","subnet"});
    add("/api/network/dns/config",  "GET",  true,  {});
    add("/api/network/dns/config",  "POST", true,  {"dns1","dns2","use_custom_dns"});
    add("/api/network/mdns",        "GET",  false, {});
    add("/api/network/mdns",        "POST", true,  {"mdns"});
    add("/api/network/public-ip",   "GET",  false, {});
    add("/api/network/public-ip/check","POST",true,  {"idx"});
    add("/api/ntp/time",            "GET",  false, {});
    add("/api/ntp/config",          "GET",  false, {});
    add("/api/ntp/config",          "POST", true,  {"server","tz_offset"});
    add("/api/gpio",                "GET",  true,  {});
    add("/api/gpio/read",           "GET",  true,  {"pin"});
    add("/api/gpio/set",            "POST", true,  {"pin","mode","value"});
    add("/api/gpio/blink",          "POST", true,  {"pin","times","ms"});
    add("/api/gpio/pulse",          "POST", true,  {"pin","ms"});
    add("/api/gpio/toggle",         "POST", true,  {"pin"});
    add("/api/gpio/analog",         "GET",  true,  {});
    add("/api/gpio/analog",         "POST", true,  {"pin","value"});
    add("/api/gpio/config/invert",  "GET",  false, {});
    add("/api/gpio/config/invert",  "POST", true,  {"enabled"});
    add("/api/gpio/config/pwm-pin", "GET",  false, {});
    add("/api/gpio/config/pwm-pin", "POST", true,  {"pin"});
    add("/api/ddns/status",         "GET",  false, {});
    add("/api/ddns/check",          "POST", true,  {});
    add("/api/ddns/update",         "POST", true,  {});
    add("/api/ddns/config",         "GET",  false, {});
    add("/api/ddns/config",         "POST", true,  {"hostname","domain","token","upd_url"});
    add("/api/ddns/interval",       "POST", true,  {"interval"});
    add("/api/ddns/ip-urls",        "GET",  false, {});
    add("/api/ddns/ip-urls",        "POST", true,  {"urls"});
    add("/api/ddns/led/pin",        "GET",  false, {});
    add("/api/ddns/led/pin",        "POST", true,  {"pin"});
    add("/api/ping",                "POST", true,  {"host"});
    add("/api/dns/resolve",         "POST", true,  {"host"});
    add("/api/http/fetch",          "POST", true,  {"url"});
    add("/api/config/export",       "GET",  true,  {});
    add("/api/config/import",       "POST", true,  {"config"});
    add("/api/info",                "GET",  false, {});
    add("/api/system/version",      "GET",  false, {});
    add("/api/system/ota",          "GET",  false, {});
    add("/api/system/ota",          "POST", true,  {"url","interval"});
    add("/api/system/ota/check",    "POST", true,  {});
    add("/api/wifi/networks",       "GET",  true,  {});
    add("/api/wifi/networks",       "POST", true,  {"ssid","password"});
    add("/api/wifi/networks",       "DELETE",true, {"index"});
    add("/api/wifi/networks/reorder","POST", true,  {"from","to"});
    add("/api/wifi/connect",        "POST", true,  {"index"});
    add("/api/wifi/retry",          "GET",  false, {});
    add("/api/wifi/retry",          "POST", true,  {"count"});
    add("/api/wifi/ap",             "GET",  false, {});
    add("/api/wifi/ap",             "POST", true,  {"enabled","ssid","password","ap_fallback"});
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/info");
  server.on("/api/info", []() {
    sendCORS();
    DynamicJsonDocument doc(1024);
    doc["mac"] = WiFi.macAddress();
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["rssi"] = WiFi.RSSI();
    doc["channel"] = WiFi.channel();
    { uint8_t phy = WiFi.getPhyMode(); doc["phy_mode"] = (phy == 2 ? "11g" : phy == 3 ? "11n" : "11b"); }
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
    doc["gpio_invert"] = cfg.gpio_invert;
    doc["led_pin"] = cfg.led_pin;
    doc["build_date"] = __DATE__;
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
    doc["ap_ssid"] = cfg.ap_ssid;
    doc["ap_fallback"] = cfg.ap_fallback;
    doc["wifi_retry_count"] = cfg.wifi_retry_count;
    doc["ota_url"] = cfg.ota_url;
    doc["ota_check_interval"] = cfg.ota_check_interval;
    doc["last_check_elapsed_ms"] = (long)(lastCheckTime > 0 ? millis() - lastCheckTime : -1);
    doc["last_check_match"] = lastCheckMatch;
    doc["last_check_public_ip"] = lastCheckedPublicIP;
    doc["last_check_ddns_ip"] = lastCheckedDomainIP;
    String ddnsIp = getCachedDDNSIP();
    if (ddnsIp != "") doc["ddns_ip"] = ddnsIp;
    String pubIp = getCachedPublicIP();
    if (pubIp != "") doc["public_ip"] = pubIp;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
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
    if (hostname != "") strncpy(cfg.ddns_hostname, hostname.c_str(), sizeof(cfg.ddns_hostname) - 1); cfg.ddns_hostname[sizeof(cfg.ddns_hostname) - 1] = '\0';
    if (domain != "") strncpy(cfg.ddns_domain, domain.c_str(), sizeof(cfg.ddns_domain) - 1); cfg.ddns_domain[sizeof(cfg.ddns_domain) - 1] = '\0';
    if (token != "") strncpy(cfg.ddns_token, token.c_str(), sizeof(cfg.ddns_token) - 1); cfg.ddns_token[sizeof(cfg.ddns_token) - 1] = '\0';
    if (upd_url != "") strncpy(cfg.ddns_upd_url, upd_url.c_str(), sizeof(cfg.ddns_upd_url) - 1); cfg.ddns_upd_url[sizeof(cfg.ddns_upd_url) - 1] = '\0';
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"ddns_hostname\":\"" + String(cfg.ddns_hostname) + "\",\"ddns_domain\":\"" + String(cfg.ddns_domain) + "\"}");
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
    DynamicJsonDocument doc(512);
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = __DATE__;
    doc["build_time"] = __TIME__;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/config/export");
  server.on("/api/config/export", []() {
    if (!checkAuth()) return;
    DynamicJsonDocument doc(2048);
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
    doc["gpio_invert"] = cfg.gpio_invert;
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
    doc["ap_ssid"] = cfg.ap_ssid;
    doc["ap_fallback"] = cfg.ap_fallback;
    doc["wifi_retry_count"] = cfg.wifi_retry_count;
    doc["ota_url"] = cfg.ota_url;
    doc["ota_check_interval"] = cfg.ota_check_interval;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/config/import");
  server.on("/api/config/import", HTTP_POST, []() {
    if (!checkAuth()) return;
    String body = server.arg("config");
    if (body == "") { server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}"); return; }
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err) { server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
    auto setStr = [&](const char* key, char* dest, size_t sz) {
      if (doc[key].is<const char*>()) { strncpy(dest, doc[key], sz - 1); dest[sz - 1] = '\0'; }
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
    setInt("gpio_invert", cfg.gpio_invert);
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
    setInt("wifi_retry_count", cfg.wifi_retry_count);
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

  addOptions("/api/gpio/blink");
  server.on("/api/gpio/blink", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    int times = server.arg("times").toInt();
    int ms = server.arg("ms").toInt();
    if (ms < 10 || ms > 5000) ms = 200;
    server.send(200, "application/json", gpioBlink(pin, times, ms));
  });

  // ═══════════════════════════════════════════════════════════════
  // NEW REST API ROUTES — alongside old ones for now
  // ═══════════════════════════════════════════════════════════════

  // ── /api/status (compact overview) ──
  addOptions("/api/status");
  server.on("/api/status", []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["ssid"] = WiFi.SSID();
    doc["local_ip"] = WiFi.localIP().toString();
    doc["ddns_match"] = lastCheckMatch;
    doc["rssi"] = WiFi.RSSI();
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  // ── SYSTEM ──
  addOptions("/api/system/status");
  server.on("/api/system/status", []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = __DATE__;
    doc["build_time"] = __TIME__;
    doc["mac"] = WiFi.macAddress();
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["last_check_elapsed_ms"] = (long)(lastCheckTime > 0 ? millis() - lastCheckTime : -1);
    doc["last_check_match"] = lastCheckMatch;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/system/log");
  server.on("/api/system/log", []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", logGet());
  });

  // ── NETWORK ──
  addOptions("/api/network/status");
  server.on("/api/network/status", []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["ssid"] = WiFi.SSID();
    doc["bssid"] = WiFi.BSSIDstr();
    doc["rssi"] = WiFi.RSSI();
    doc["channel"] = WiFi.channel();
    uint8_t phy = WiFi.getPhyMode();
    const char* phyStr = "11b";
    if (phy == 2) phyStr = "11g"; else if (phy == 3) phyStr = "11n";
    doc["phy_mode"] = phyStr;
    doc["mac"] = WiFi.macAddress();
    doc["local_ip"] = WiFi.localIP().toString();
    doc["gateway"] = WiFi.gatewayIP().toString();
    doc["subnet"] = WiFi.subnetMask().toString();
    doc["dns1"] = WiFi.dnsIP(0).toString();
    doc["dns2"] = WiFi.dnsIP(1).toString();
    doc["hostname"] = WiFi.hostname();
    doc["mdns"] = cfg.mdns_name;
    doc["wifi_status"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
    doc["auto_reconnect"] = WiFi.getAutoReconnect();
    doc["use_static_ip"] = cfg.use_static_ip;
    doc["public_ip"] = getCachedPublicIP();
    doc["ap_active"] = wifiAPRunning();
    doc["ap_ssid"] = wifiAPSSID();
    doc["ap_clients"] = wifiAPClientCount();
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/network/wifi/scan");
  server.on("/api/network/wifi/scan", []() {
    if (!checkAuth()) return;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); server.send(200, "application/json", "{\"status\":\"scanning\"}"); }
    else if (n == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "{\"status\":\"scanning\"}"); }
    else {
      DynamicJsonDocument doc(1024); JsonArray networks = doc["networks"].to<JsonArray>();
      for (int i = 0; i < n; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i); net["rssi"] = WiFi.RSSI(i);
        net["channel"] = WiFi.channel(i); net["encryption"] = WiFi.encryptionType(i);
        uint8_t* bssid = WiFi.BSSID(i);
        if (bssid) {
          char bssidStr[18]; snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
          net["bssid"] = bssidStr;
        }
      }
      String r; serializeJson(doc, r); WiFi.scanDelete();
      server.send(200, "application/json", r);
    }
  });

  addOptions("/api/network/wifi/ssid");
  server.on("/api/network/wifi/ssid", HTTP_POST, []() {
    if (!checkAuth()) return;
    String s = server.arg("ssid");
    if (s == "") { server.send(400, "application/json", "{\"error\":\"Missing ssid\"}"); return; }
    strncpy(cfg.wifi_ssid, s.c_str(), sizeof(cfg.wifi_ssid) - 1); cfg.wifi_ssid[sizeof(cfg.wifi_ssid) - 1] = '\0';
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"ssid\":\"" + String(cfg.wifi_ssid) + "\"}");
  });

  addOptions("/api/network/wifi/password");
  server.on("/api/network/wifi/password", HTTP_POST, []() {
    if (!checkAuth()) return;
    String p = server.arg("password");
    if (p == "") { server.send(400, "application/json", "{\"error\":\"Missing password\"}"); return; }
    strncpy(cfg.wifi_password, p.c_str(), sizeof(cfg.wifi_password) - 1); cfg.wifi_password[sizeof(cfg.wifi_password) - 1] = '\0';
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  });

  addOptions("/api/network/ip/config");
  server.on("/api/network/ip/config", HTTP_GET, []() {
    if (!checkAuth()) return;
    DynamicJsonDocument doc(512);
    doc["use_static_ip"] = cfg.use_static_ip;
    doc["ip"] = cfg.static_ip;
    doc["gateway"] = cfg.static_gateway;
    doc["subnet"] = cfg.static_subnet;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/network/ip/config", HTTP_POST, []() {
    if (!checkAuth()) return;
    String dhcp = server.arg("dhcp");
    cfg.use_static_ip = (dhcp == "0" || dhcp == "false") ? 1 : 0;
    if (cfg.use_static_ip) {
      if (server.arg("ip") != "") { strncpy(cfg.static_ip, server.arg("ip").c_str(), sizeof(cfg.static_ip) - 1); cfg.static_ip[sizeof(cfg.static_ip) - 1] = '\0'; }
      if (server.arg("gateway") != "") { strncpy(cfg.static_gateway, server.arg("gateway").c_str(), sizeof(cfg.static_gateway) - 1); cfg.static_gateway[sizeof(cfg.static_gateway) - 1] = '\0'; }
      if (server.arg("subnet") != "") { strncpy(cfg.static_subnet, server.arg("subnet").c_str(), sizeof(cfg.static_subnet) - 1); cfg.static_subnet[sizeof(cfg.static_subnet) - 1] = '\0'; }
    }
    storageSave();
    applyNetworkConfig();
    bool valid = validateNetworkConfig();
    DynamicJsonDocument doc(512); doc["status"] = "saved"; doc["use_static_ip"] = cfg.use_static_ip;
    if (!valid) doc["warning"] = "config invalid, switched to DHCP";
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/network/dns/config");
  server.on("/api/network/dns/config", HTTP_GET, []() {
    if (!checkAuth()) return;
    DynamicJsonDocument doc(512);
    doc["dns1"] = cfg.static_dns1;
    doc["dns2"] = cfg.static_dns2;
    doc["use_custom_dns"] = cfg.use_custom_dns;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/network/dns/config", HTTP_POST, []() {
    if (!checkAuth()) return;
    if (server.arg("dns1") != "") { strncpy(cfg.static_dns1, server.arg("dns1").c_str(), sizeof(cfg.static_dns1) - 1); cfg.static_dns1[sizeof(cfg.static_dns1) - 1] = '\0'; }
    if (server.arg("dns2") != "") { strncpy(cfg.static_dns2, server.arg("dns2").c_str(), sizeof(cfg.static_dns2) - 1); cfg.static_dns2[sizeof(cfg.static_dns2) - 1] = '\0'; }
    String ucd = server.arg("use_custom_dns");
    if (ucd != "") cfg.use_custom_dns = (ucd == "1" || ucd == "true") ? 1 : 0;
    storageSave();
    if (cfg.use_custom_dns) { applyNetworkConfig(); }
    else { WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask()); }
    bool valid = validateNetworkConfig();
    DynamicJsonDocument doc(512); doc["status"] = "saved"; doc["dns1"] = cfg.static_dns1; doc["dns2"] = cfg.static_dns2; doc["use_custom_dns"] = cfg.use_custom_dns;
    if (!valid) doc["warning"] = "config invalid, switched to DHCP";
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/network/mdns");
  server.on("/api/network/mdns", HTTP_GET, []() {
    sendCORS();
    server.send(200, "application/json", "{\"mdns\":\"" + String(cfg.mdns_name) + "\"}");
  });
  server.on("/api/network/mdns", HTTP_POST, []() {
    if (!checkAuth()) return;
    String m = server.arg("mdns");
    if (m == "") { server.send(400, "application/json", "{\"error\":\"Missing mdns\"}"); return; }
    strncpy(cfg.mdns_name, m.c_str(), sizeof(cfg.mdns_name) - 1);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"mdns\":\"" + String(cfg.mdns_name) + "\"}");
  });

  addOptions("/api/network/public-ip");
  server.on("/api/network/public-ip", HTTP_GET, []() {
    sendCORS();
    String ip = getCachedPublicIP();
    if (ip != "") { server.send(200, "application/json", "{\"public_ip\":\"" + ip + "\"}"); }
    else { server.send(200, "application/json", "{\"public_ip\":null}"); }
  });

  addOptions("/api/network/public-ip/check");
  server.on("/api/network/public-ip/check", HTTP_POST, []() {
    if (!checkAuth()) return;
    String idxStr = server.arg("idx");
    String ip, serverUsed; bool success = false;
    if (idxStr != "") {
      int idx = idxStr.toInt();
      String urls = String(cfg.public_ip_urls); int cur = 0;
      while (urls.length() > 0) {
        int comma = urls.indexOf(','); String host = (comma == -1) ? urls : urls.substring(0, comma);
        urls = (comma == -1) ? "" : urls.substring(comma + 1); host.trim();
        if (host == "") continue;
        if (cur == idx) { serverUsed = host; break; } cur++;
      }
      ip = getPublicIP(idx); if (ip != "") success = true;
    } else {
      ip = getPublicIP(); if (ip != "") success = true;
      String urls = String(cfg.public_ip_urls);
      int comma = urls.indexOf(','); serverUsed = (comma == -1) ? urls : urls.substring(0, comma); serverUsed.trim();
    }
    DynamicJsonDocument doc(512); doc["ip"] = ip; doc["server"] = serverUsed; doc["success"] = success;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  // ── NTP ──
  addOptions("/api/ntp/config");
  server.on("/api/ntp/config", HTTP_GET, []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["ntp_server"] = cfg.ntp_server;
    doc["tz_offset"] = cfg.tz_offset;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/ntp/config", HTTP_POST, []() {
    if (!checkAuth()) return;
    if (server.arg("server") != "") strncpy(cfg.ntp_server, server.arg("server").c_str(), sizeof(cfg.ntp_server) - 1);
    if (server.arg("tz_offset") != "") cfg.tz_offset = server.arg("tz_offset").toInt();
    storageSave();
    DynamicJsonDocument doc(512); doc["status"] = "saved"; doc["ntp_server"] = cfg.ntp_server; doc["tz_offset"] = cfg.tz_offset;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/ntp/time");
  server.on("/api/ntp/time", []() {
    sendCORS();
    time_t epoch = ntpGetEpoch();
    int tz_hours = cfg.tz_offset / 3600;
    char utc_str[16];
    snprintf(utc_str, sizeof(utc_str), "UTC%+dh", tz_hours);
    DynamicJsonDocument doc(512);
    doc["time"] = ntpGetTime();
    doc["epoch"] = (long)epoch;
    doc["tz_offset"] = cfg.tz_offset;
    doc["utc"] = utc_str;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  // ── GPIO (new paths, all with consistent auth) ──
  addOptions("/api/gpio");
  server.on("/api/gpio", []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", gpioInfo());
  });

  addOptions("/api/gpio/read");
  server.on("/api/gpio/read", []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    server.send(200, "application/json", gpioRead(pin));
  });

  addOptions("/api/gpio/set");
  server.on("/api/gpio/set", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt(); String mode = server.arg("mode"); String value = server.arg("value");
    server.send(200, "application/json", gpioSet(pin, mode, value));
  });

  addOptions("/api/gpio/blink");
  server.on("/api/gpio/blink", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt(); int times = server.arg("times").toInt();
    int ms = server.arg("ms").toInt(); if (ms < 10 || ms > 5000) ms = 200;
    server.send(200, "application/json", gpioBlink(pin, times, ms));
  });

  addOptions("/api/gpio/pulse");
  server.on("/api/gpio/pulse", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt(); int ms = server.arg("ms").toInt();
    if (ms < 1 || ms > 10000) ms = 500;
    server.send(200, "application/json", gpioPulse(pin, ms));
  });

  addOptions("/api/gpio/toggle");
  server.on("/api/gpio/toggle", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    server.send(200, "application/json", gpioToggle(pin));
  });

  addOptions("/api/gpio/analog");
  server.on("/api/gpio/analog", HTTP_GET, []() {
    if (!checkAuth()) return;
    server.send(200, "application/json", analogReadPin());
  });
  server.on("/api/gpio/analog", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt(); int value = server.arg("value").toInt();
    server.send(200, "application/json", analogWritePin(pin, value));
  });

  addOptions("/api/gpio/config/invert");
  server.on("/api/gpio/config/invert", HTTP_GET, []() {
    sendCORS();
    server.send(200, "application/json", "{\"gpio_invert\":" + String(cfg.gpio_invert) + "}");
  });
  server.on("/api/gpio/config/invert", HTTP_POST, []() {
    if (!checkAuth()) return;
    String enabled = server.arg("enabled");
    cfg.gpio_invert = (enabled == "1" || enabled == "true") ? 1 : 0;
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"gpio_invert\":" + String(cfg.gpio_invert) + "}");
  });

  addOptions("/api/gpio/config/pwm-pin");
  server.on("/api/gpio/config/pwm-pin", HTTP_GET, []() {
    sendCORS();
    server.send(200, "application/json", "{\"pwm_pin\":" + String(cfg.pwm_pin) + "}");
  });
  server.on("/api/gpio/config/pwm-pin", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    if (pin < 0 || pin > 16) { server.send(400, "application/json", "{\"error\":\"Invalid pin\"}"); return; }
    cfg.pwm_pin = pin; storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"pwm_pin\":" + String(cfg.pwm_pin) + "}");
  });

  // ── DDNS ──
  addOptions("/api/ddns/status");
  server.on("/api/ddns/status", []() {
    sendCORS();
    String r = ddnsCheck();
    server.send(200, "application/json", r);
  });

  addOptions("/api/ddns/check");
  server.on("/api/ddns/check", HTTP_POST, []() {
    if (!checkAuth()) return;
    bool match = checkDDNS();
    DynamicJsonDocument doc(512);
    doc["match"] = match;
    doc["public_ip"] = lastCheckedPublicIP;
    doc["ddns_ip"] = lastCheckedDomainIP;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/ddns/config");
  server.on("/api/ddns/config", HTTP_GET, []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["hostname"] = cfg.ddns_hostname;
    doc["domain"] = cfg.ddns_domain;
    doc["upd_url"] = cfg.ddns_upd_url;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });

  addOptions("/api/ddns/ip-urls");
  server.on("/api/ddns/ip-urls", HTTP_GET, []() {
    sendCORS();
    server.send(200, "application/json", "{\"urls\":\"" + String(cfg.public_ip_urls) + "\"}");
  });
  server.on("/api/ddns/ip-urls", HTTP_POST, []() {
    if (!checkAuth()) return;
    String urls = server.arg("urls");
    if (urls != "") { urls.replace("https://", ""); urls.replace("http://", ""); strncpy(cfg.public_ip_urls, urls.c_str(), sizeof(cfg.public_ip_urls) - 1); }
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"urls\":\"" + String(cfg.public_ip_urls) + "\"}");
  });

  addOptions("/api/ddns/led/pin");
  server.on("/api/ddns/led/pin", HTTP_GET, []() {
    sendCORS();
    server.send(200, "application/json", "{\"led_pin\":" + String(cfg.led_pin) + ",\"led_on\":false}");
  });
  server.on("/api/ddns/led/pin", HTTP_POST, []() {
    if (!checkAuth()) return;
    int pin = server.arg("pin").toInt();
    if (pin < 0 || pin > 16) { server.send(400, "application/json", "{\"error\":\"Invalid pin\"}"); return; }
    cfg.led_pin = pin; pinMode(cfg.led_pin, OUTPUT);
    digitalWrite(cfg.led_pin, cfg.gpio_invert ? LOW : HIGH);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"led_pin\":" + String(cfg.led_pin) + "}");
  });

  // ── TOOLS ──
  addOptions("/api/ping");
  server.on("/api/ping", HTTP_POST, []() {
    if (!checkAuth()) return;
    String host = server.arg("host");
    if (host == "") { server.send(400, "application/json", "{\"error\":\"Missing host\"}"); return; }
    String result = pingHost(host);
    server.send(200, "application/json", result);
  });

  addOptions("/api/dns/resolve");
  server.on("/api/dns/resolve", HTTP_POST, []() {
    if (!checkAuth()) return;
    String host = server.arg("host");
    if (host == "") { server.send(400, "application/json", "{\"error\":\"Missing host\"}"); return; }
    IPAddress ip;
    if (WiFi.hostByName(host.c_str(), ip)) {
      server.send(200, "application/json", "{\"host\":\"" + host + "\",\"ip\":\"" + ip.toString() + "\"}");
    } else {
      server.send(500, "application/json", "{\"error\":\"DNS failed\"}");
    }
  });

  addOptions("/api/http/fetch");
  server.on("/api/http/fetch", HTTP_POST, []() {
    if (!checkAuth()) return;
    String url = server.arg("url");
    if (url == "") { server.send(400, "application/json", "{\"error\":\"Missing url\"}"); return; }
    if (!url.startsWith("http://") && !url.startsWith("https://")) { server.send(400, "application/json", "{\"error\":\"Invalid protocol\"}"); return; }
    if (WiFi.status() != WL_CONNECTED) { server.send(503, "application/json", "{\"error\":\"WiFi not connected\"}"); return; }
    HTTPClient http; int code; String payload;
    http.setTimeout(10000);
    http.setUserAgent("ESP-DDNS/1.0");
    WiFiClientSecure cSecure; WiFiClient cPlain;
    bool isHttps = url.startsWith("https://");
    if (isHttps) { cSecure.setInsecure(); http.begin(cSecure, url); }
    else { http.begin(cPlain, url); }
    code = http.GET();
    auto readPayload = [&]() {
      WiFiClient *s = http.getStreamPtr();
      if (!s) return;
      int maxSize = 10240, total = 0; unsigned long timeout = millis() + 5000;
      while (millis() < timeout && total < maxSize) {
        if (s->available()) { payload += (char)s->read(); total++; timeout = millis() + 5000; }
      }
      if (total >= maxSize) payload += "\n[TRUNCATED at 10KB]";
    };
    if (code == HTTP_CODE_OK) { readPayload(); }
    else if (isHttps) {
      http.end();
      String httpUrl = "http://" + url.substring(8);
      http.begin(cPlain, httpUrl);
      code = http.GET();
      if (code == HTTP_CODE_OK) { readPayload(); }
      else { payload = "Error (" + String(code) + "): " + http.errorToString(code); }
    } else { payload = "Error (" + String(code) + "): " + http.errorToString(code); }
    http.end();
    server.send(200, "text/plain", payload);
  });

  // ── WIFI NETWORKS ──
  addOptions("/api/wifi/networks");
  server.on("/api/wifi/networks", HTTP_GET, []() {
    if (!checkAuth()) return;
    DynamicJsonDocument doc(512);
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (int i = 0; i < networkListCount(); i++) {
      WifiNetwork net;
      if (networkListGet(i, net)) {
        JsonObject o = arr.createNestedObject();
        o["ssid"] = net.ssid;
        o["index"] = i;
      }
    }
    doc["count"] = networkListCount();
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/wifi/networks", HTTP_POST, []() {
    if (!checkAuth()) return;
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    if (ssid == "") { server.send(400, "application/json", "{\"error\":\"Missing ssid\"}"); return; }
    if (!networkListAdd(ssid.c_str(), password.c_str())) {
      server.send(400, "application/json", "{\"error\":\"Max networks reached\"}");
      return;
    }
    server.send(200, "application/json", "{\"status\":\"added\",\"count\":" + String(networkListCount()) + "}");
  });
  server.on("/api/wifi/networks", HTTP_DELETE, []() {
    if (!checkAuth()) return;
    int idx = server.arg("index").toInt();
    if (!networkListRemove(idx)) {
      server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
      return;
    }
    server.send(200, "application/json", "{\"status\":\"removed\",\"count\":" + String(networkListCount()) + "}");
  });

  addOptions("/api/wifi/networks/reorder");
  server.on("/api/wifi/networks/reorder", HTTP_POST, []() {
    if (!checkAuth()) return;
    int from = server.arg("from").toInt();
    int to = server.arg("to").toInt();
    if (!networkListMove(from, to)) {
      server.send(400, "application/json", "{\"error\":\"Invalid indices\"}");
      return;
    }
    server.send(200, "application/json", "{\"status\":\"reordered\"}");
  });

  addOptions("/api/wifi/connect");
  server.on("/api/wifi/connect", HTTP_POST, []() {
    if (!checkAuth()) return;
    int idx = server.arg("index").toInt();
    WifiNetwork net;
    if (!networkListGet(idx, net)) {
      server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
      return;
    }
    WiFi.disconnect();
    delay(100);
    int retries = cfg.wifi_retry_count > 0 ? cfg.wifi_retry_count : 1;
    bool connected = false;
    for (int t = 0; t < retries; t++) {
      WiFi.begin(net.ssid, net.password);
      for (int w = 0; w < 50; w++) {
        if (WiFi.status() == WL_CONNECTED) { connected = true; break; }
        delay(200);
      }
      if (connected) break;
    }
    server.send(200, "application/json", "{\"status\":\"" + String(connected ? "connected" : "connecting") + "\",\"ssid\":\"" + String(net.ssid) + "\"}");
  });

  // ── WIFI AP ──
  addOptions("/api/wifi/ap");
  server.on("/api/wifi/ap", HTTP_GET, []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["active"] = wifiAPRunning();
    doc["ssid"] = wifiAPSSID();
    doc["ip"] = wifiAPIP();
    doc["clients"] = wifiAPClientCount();
    doc["configured_ssid"] = cfg.ap_ssid;
    doc["ap_fallback"] = cfg.ap_fallback;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/wifi/ap", HTTP_POST, []() {
    if (!checkAuth()) return;
    String enabled = server.arg("enabled");
    if (enabled != "") {
      if (enabled == "1" || enabled == "true") {
        wifiAPStart();
        server.send(200, "application/json", "{\"status\":\"ap_started\"}");
      } else {
        wifiAPStop();
        server.send(200, "application/json", "{\"status\":\"ap_stopped\"}");
      }
      return;
    }
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String fallback = server.arg("ap_fallback");
    if (ssid != "") strncpy(cfg.ap_ssid, ssid.c_str(), sizeof(cfg.ap_ssid) - 1);
    if (password != "") strncpy(cfg.ap_password, password.c_str(), sizeof(cfg.ap_password) - 1);
    if (fallback != "") cfg.ap_fallback = (fallback == "1" || fallback == "true") ? 1 : 0;
    storageSave();
    if (wifiAPRunning()) {
      wifiAPStop();
      wifiAPStart();
    }
    server.send(200, "application/json", "{\"status\":\"ap_config_saved\"}");
  });

  // ── WIFI RETRY ──
  addOptions("/api/wifi/retry");
  server.on("/api/wifi/retry", HTTP_GET, []() {
    sendCORS();
    DynamicJsonDocument doc(128);
    doc["wifi_retry_count"] = cfg.wifi_retry_count;
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/wifi/retry", HTTP_POST, []() {
    if (!checkAuth()) return;
    int count = server.arg("count").toInt();
    if (count < 1 || count > 10) { server.send(400, "application/json", "{\"error\":\"Count must be 1-10\"}"); return; }
    cfg.wifi_retry_count = count;
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"wifi_retry_count\":" + String(cfg.wifi_retry_count) + "}");
  });

  // ── SYSTEM OTA ──
  addOptions("/api/system/ota");
  server.on("/api/system/ota", HTTP_GET, []() {
    sendCORS();
    DynamicJsonDocument doc(512);
    doc["ota_url"] = cfg.ota_url;
    doc["ota_interval"] = cfg.ota_check_interval;
    doc["current_version"] = FIRMWARE_VERSION;
    unsigned long last = otaLastCheckMs();
    if (last > 0) doc["last_check_elapsed"] = (long)((millis() - last) / 1000);
    String r; serializeJson(doc, r);
    server.send(200, "application/json", r);
  });
  server.on("/api/system/ota", HTTP_POST, []() {
    if (!checkAuth()) return;
    String url = server.arg("url");
    String interval = server.arg("interval");
    cfg.ota_url[0] = '\0';
    if (url != "") strncpy(cfg.ota_url, url.c_str(), sizeof(cfg.ota_url) - 1);
    if (interval != "") cfg.ota_check_interval = interval.toInt();
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  });
  addOptions("/api/system/ota/check");
  server.on("/api/system/ota/check", HTTP_POST, []() {
    if (!checkAuth()) return;
    bool ok = otaCheckNow();
    if (ok) {
      server.send(200, "application/json", "{\"updating\":true}");
    } else {
      server.send(200, "application/json", "{\"updating\":false,\"error\":\"" + otaLastError() + "\"}");
    }
  });

  server.begin();
  logPrint("HTTP", "server started");
}
