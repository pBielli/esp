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
bool flag_firstRun = true;
void setup() {
  Serial.begin(115200);
  Serial.println("\n");

  storageBegin();
  storageLoad();
    storageReset();
  if (!storageInitialized()) {
    storageReset();
    logAdd(millis(), "Config reset to default");
  }

  pinMode(cfg.led_pin, OUTPUT);
  digitalWrite(cfg.led_pin, cfg.led_invert ? LOW : HIGH);

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" " + WiFi.localIP().toString());
  logAdd(millis(), "WiFi connected: " + WiFi.localIP().toString());

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
    Serial.print("Reconnecting WiFi");
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" OK " + WiFi.localIP().toString());
    logAdd(millis(), "WiFi reconnected: " + WiFi.localIP().toString());
    applyNetworkConfig();
  }

  if (millis() - lastCheck > 300000 || flag_firstRun) { // Check every 5 minutes
    if(flag_firstRun){
      flag_firstRun = false;
      IPAddress test_ip;
      Serial.println("Checking DDNS hostname: " + String(cfg.ddns_hostname));
      if (WiFi.hostByName(cfg.ddns_hostname, test_ip)) {
        Serial.println("TEST:DDNS IP: " + test_ip.toString());
      } else {
        Serial.println("TEST:Failed to resolve DDNS hostname");
      }
    }
    lastCheck = millis();
    checkAndUpdateDDNS();
  }
}
