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
bool flag_firtstRun = true;
int count_ddns_update_failures=0;
void setup() {
  Serial.begin(115200);
  Serial.println("\n");

  storageBegin();
  storageLoad();
  if (!storageInitialized()) {
    storageReset();
    logPrint("CONFIG", "reset to default");
  }

  pinMode(cfg.led_pin, OUTPUT);
  digitalWrite(cfg.led_pin, cfg.led_invert ? LOW : HIGH);

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  logPrint("WIFI", "Connecting to " + String(cfg.wifi_ssid));
  int wifiTimeout = 40;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout > 0) { delay(500); Serial.print("."); wifiTimeout--; }
  if (WiFi.status() == WL_CONNECTED) {
    logPrint("WIFI", "Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println(" FAILED (will retry in loop)");
  }

  applyNetworkConfig();
  validateNetworkConfig();

  ntpBegin();
  logPrint("NTP", "Time: " + ntpGetTime());

  if (MDNS.begin(cfg.mdns_name)) {
    logPrint("mDNS", String(cfg.mdns_name) + ".local");
  }

  setupRoutes();
  logPrint("SYS", "System started");
  lastCheck = millis();
}

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWifiRetry = 0;
    if (millis() - lastWifiRetry > 30000) {
      lastWifiRetry = millis();
      logPrint("WIFI", "Reconnecting to " + String(cfg.wifi_ssid));
      WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
      int wifiTimeout = 20;
      while (WiFi.status() != WL_CONNECTED && wifiTimeout > 0) { delay(500); Serial.print("."); wifiTimeout--; }
        Serial.println("");
      if (WiFi.status() == WL_CONNECTED) {
        logPrint("WIFI", "Reconnected: " + WiFi.localIP().toString());
        if (cfg.use_static_ip) {
          IPAddress ip, gw, subnet, dns1, dns2;
          ip.fromString(cfg.static_ip);
          gw.fromString(cfg.static_gateway);
          subnet.fromString(cfg.static_subnet);
          dns1.fromString(cfg.static_dns1);
          dns2.fromString(cfg.static_dns2);
          WiFi.config(ip, gw, subnet, dns1, dns2);
        } else if (cfg.use_custom_dns) {
          String ipStr = WiFi.localIP().toString();
          String gwStr = WiFi.gatewayIP().toString();
          String subnetStr = WiFi.subnetMask().toString();
          strncpy(cfg.static_ip, ipStr.c_str(), sizeof(cfg.static_ip) - 1);
          strncpy(cfg.static_gateway, gwStr.c_str(), sizeof(cfg.static_gateway) - 1);
          strncpy(cfg.static_subnet, subnetStr.c_str(), sizeof(cfg.static_subnet) - 1);
          storageSave();
          IPAddress ip, gw, subnet, dns1, dns2;
          ip.fromString(cfg.static_ip);
          gw.fromString(cfg.static_gateway);
          subnet.fromString(cfg.static_subnet);
          dns1.fromString(cfg.static_dns1);
          dns2.fromString(cfg.static_dns2);
          WiFi.config(ip, gw, subnet, dns1, dns2);
          logPrint("DNS", "Custom DNS: " + String(cfg.static_ip) + " / " + String(cfg.static_dns1) + ", " + String(cfg.static_dns2));
        }
        validateNetworkConfig();
      }
    }
  }

  if (millis() - lastCheck > (unsigned long)cfg.ddns_check_interval * 1000 || flag_firtstRun) {
    if(flag_firtstRun)
      flag_firtstRun = false;
    //controlla n volte, se non va allora aggiorna, se va allora è tutto ok, se non va dopo aggiornamento allora c'è un problema e blinka in continuo, altrimenti se risolve prima allora è tutto ok resetta contatore e ricomincia
    if(checkDDNS())
      count_ddns_update_failures=0;
    else {
      count_ddns_update_failures++;
      if(count_ddns_update_failures>=3){
        logPrint("DDNS", "Check failed " + String(count_ddns_update_failures) + " times");
        ledBlink(count_ddns_update_failures);
      } else {
        logPrint("DDNS", "Check failed " + String(count_ddns_update_failures) + " times, will update");
        checkAndUpdateDDNS();
      }}
    lastCheck = millis();
      }
}
