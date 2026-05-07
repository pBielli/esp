#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <time.h>

#include "Config.h"
#include "Logger.h"
#include "NTP.h"
#include "GPIO.h"
#include "DDNS.h"
#include "WebServer.h"

unsigned long lastCheck = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n");

  storageBegin();
  storageLoad();
  if (!storageInitialized()) {
    storageReset();
    logAdd(millis(), "Config reset to default");
  }

  pinMode(cfg.led_pin, OUTPUT);
  digitalWrite(cfg.led_pin, cfg.led_invert ? LOW : HIGH);

  if (cfg.use_static_ip) {
    IPAddress ip, gw, subnet, dns1, dns2;
    ip.fromString(cfg.static_ip);
    gw.fromString(cfg.static_gateway);
    subnet.fromString(cfg.static_subnet);
    dns1.fromString(cfg.static_dns1);
    dns2.fromString(cfg.static_dns2);
    WiFi.config(ip, gw, subnet, dns1, dns2);
  }

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" " + WiFi.localIP().toString());
  logAdd(millis(), "WiFi connected: " + WiFi.localIP().toString());

  if (!cfg.use_static_ip) {
    IPAddress dns1, dns2;
    if (dns1.fromString(cfg.static_dns1) || dns2.fromString(cfg.static_dns2)) {
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
    }
  }

  ntpBegin();
  logAdd(millis(), "Time: " + ntpGetTime());

  if (MDNS.begin(cfg.mdns_name)) {
    Serial.println("mDNS: " + String(cfg.mdns_name) + ".local");
  }

  setupRoutes();
  logAdd(millis(), "System started");
  lastCheck = millis();
}

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Reconnecting WiFi");
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" OK " + WiFi.localIP().toString());
    logAdd(millis(), "WiFi reconnected: " + WiFi.localIP().toString());
  }

  if (millis() - lastCheck > 300000) {
    lastCheck = millis();
    checkAndUpdateDDNS();
  }
}
