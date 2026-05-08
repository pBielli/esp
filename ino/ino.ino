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
    // storageReset();
  if (!storageInitialized()) {
    storageReset();
    logAdd(millis(), "Config reset to default");
  }

  pinMode(cfg.led_pin, OUTPUT);
  digitalWrite(cfg.led_pin, cfg.led_invert ? LOW : HIGH);

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  Serial.print("WiFi: Connecting to"+String(cfg.wifi_ssid));
  int wifiTimeout = 40;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout > 0) { delay(500); Serial.print("."); wifiTimeout--; }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" " + WiFi.localIP().toString());
    logAdd(millis(), "WiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println(" FAILED (will retry in loop)");
  }

  applyNetworkConfig();

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
    static unsigned long lastWifiRetry = 0;
    if (millis() - lastWifiRetry > 30000) {
      lastWifiRetry = millis();
      Serial.print("Reconnecting WiFi");
      WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
      int wifiTimeout = 20;
      while (WiFi.status() != WL_CONNECTED && wifiTimeout > 0) { delay(500); Serial.print("."); wifiTimeout--; }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" OK " + WiFi.localIP().toString());
        logAdd(millis(), "WiFi reconnected: " + WiFi.localIP().toString());
        if (cfg.use_static_ip) {
          IPAddress ip, gw, subnet, dns1, dns2;
          ip.fromString(cfg.static_ip);
          gw.fromString(cfg.static_gateway);
          subnet.fromString(cfg.static_subnet);
          dns1.fromString(cfg.static_dns1);
          dns2.fromString(cfg.static_dns2);
          WiFi.config(ip, gw, subnet, dns1, dns2);
        } else if (cfg.use_custom_dns) {
          IPAddress dns1, dns2;
          bool ok_dns1 = dns1.fromString(cfg.static_dns1);
          bool ok_dns2 = dns2.fromString(cfg.static_dns2);
          if (ok_dns1 || ok_dns2) {
            WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
            logAdd(millis(), "Custom DNS re-applied: " + String(cfg.static_dns1) + ", " + String(cfg.static_dns2));
          }
        }
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
        Serial.println("DDNS check failed "+String(count_ddns_update_failures)+" times, blinking LED");
        logAdd(millis(), "DDNS check failed "+String(count_ddns_update_failures)+" times, blinking LED");
        while(true){
          ledBlink(1000);
        }
      } else {
        Serial.println("DDNS check failed "+String(count_ddns_update_failures)+" times, will try to update DDNS");
        logAdd(millis(), "DDNS check failed "+String(count_ddns_update_failures)+" times, will try to update DDNS");
        checkAndUpdateDDNS();
      }}
    lastCheck = millis();
      }
}
