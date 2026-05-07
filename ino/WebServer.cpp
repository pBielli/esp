#include "WebServer.h"
#include "Config.h"
#include "Logger.h"
#include "NTP.h"
#include "GPIO.h"
#include "DDNS.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "WebServer_index.h"
ESP8266WebServer server(80);

String base64Decode(String input) {
  const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out = "";
  int i = 0;
  while (i < input.length()) {
    int a = strchr(t, input[i++]) - t;
    int b = i < input.length() ? strchr(t, input[i++]) - t : 0;
    int c = i < input.length() ? strchr(t, input[i++]) - t : 0;
    int d = i < input.length() ? strchr(t, input[i++]) - t : 0;
    out += char((a << 2) | (b >> 4));
    if (i-2 >= 0 && input[i-2] != '=') out += char(((b & 15) << 4) | (c >> 2));
    if (i-1 >= 0 && input[i-1] != '=') out += char(((c & 3) << 6) | d);
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
  server.sendHeader("Access-Control-Allow-Origin", "https://pbielli.github.io");
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
    String html = "<html><head><title>" + String(cfg.mdns_name) + "</title></head><body>";
    html += "<h1>" + String(cfg.mdns_name) + "</h1>";
    html += "<p>MAC: " + WiFi.macAddress() + "</p>";
    html += "<p>SSID: " + WiFi.SSID() + "</p>";
    html += "<p>RSSI: " + String(WiFi.RSSI()) + " dBm</p>";
    html += "<p>mDNS: " + String(cfg.mdns_name) + ".local</p>";
    html += "<p>DDNS: " + String(cfg.ddns_hostname) + "</p>";
    IPAddress dip;
    if (WiFi.hostByName(cfg.ddns_hostname, dip)) html += "<p>DDNS IP: " + dip.toString() + "</p>";
    String pub = getPublicIP();
    if (pub != "") html += "<p>Public IP: " + pub + "</p>";
    html += "<p><a href='/api/help'><button>API Help</button></a></p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  // server.on("/", []() {
  //   sendCORS();
  //   String html = "<html><head><title>" + String(cfg.mdns_name) + "</title></head><body>";
  //   html += "<h1>" + String(cfg.mdns_name) + "</h1>";
  //   html += "<p>MAC: " + WiFi.macAddress() + "</p>";
  //   html += "<p>SSID: " + WiFi.SSID() + "</p>";
  //   html += "<p>RSSI: " + String(WiFi.RSSI()) + " dBm</p>";
  //   html += "<p>mDNS: " + String(cfg.mdns_name) + ".local</p>";
  //   html += "<p>DDNS: " + String(cfg.ddns_hostname) + "</p>";
  //   IPAddress dip;
  //   if (WiFi.hostByName(cfg.ddns_hostname, dip)) html += "<p>DDNS IP: " + dip.toString() + "</p>";
  //   String pub = getPublicIP();
  //   if (pub != "") html += "<p>Public IP: " + pub + "</p>";
  //   html += "<p><a href='/api/help'><button>API Help</button></a></p>";
  //   html += "</body></html>";
  //   server.send(200, "text/html", html);
  // });

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
    a.add("/api/resolve?host=x");
    a.add("/api/curl?url=x");
    a.add("/api/gpio/info");
    a.add("/api/gpio/read?pin=X");
    a.add("/api/gpio/set (POST: pin,mode,value)");
    a.add("/api/led/pin (POST)");
    a.add("/api/led/on");
    a.add("/api/led/off");
    a.add("/api/led/blink");
    a.add("/api/set/ssid (POST)");
    a.add("/api/set/pswd (POST)");
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
    doc["uptime"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    IPAddress dip;
    if (WiFi.hostByName(cfg.ddns_hostname, dip)) doc["ddns_ip"] = dip.toString();
    String pub = getPublicIP();
    if (pub != "") doc["public_ip"] = pub;
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
    server.send(200, "application/json", "{\"time\":\"" + ntpGetTime() + "\"}");
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
    digitalWrite(cfg.led_pin, HIGH);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\",\"led_pin\":" + String(cfg.led_pin) + "}");
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
    strncpy(cfg.wifi_ssid, s.c_str(), 31);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  });

  addOptions("/api/set/pswd");
  server.on("/api/set/pswd", HTTP_POST, []() {
    if (!checkAuth()) return;
    String p = server.arg("pswd");
    if (p == "") { server.send(400, "application/json", "{\"error\":\"Missing pswd\"}"); return; }
    strncpy(cfg.wifi_password, p.c_str(), 63);
    storageSave();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  });

  server.begin();
  Serial.println("HTTP server started");
}
