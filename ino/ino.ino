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
  digitalWrite(cfg.led_pin, cfg.gpio_invert ? LOW : HIGH);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  logPrint("WIFI", "Connecting to " + String(cfg.wifi_ssid));

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

  static bool wifiWasConnected = false;
  if (WiFi.status() == WL_CONNECTED && !wifiWasConnected) {
    wifiWasConnected = true;
    logPrint("WIFI", "Connected: " + WiFi.localIP().toString());
    applyNetworkConfig();
    validateNetworkConfig();
  } else if (WiFi.status() != WL_CONNECTED) {
    wifiWasConnected = false;
  }

  if (millis() - lastCheck > (unsigned long)cfg.ddns_check_interval * 1000 || flag_firtstRun) {
    if(flag_firtstRun)
      flag_firtstRun = false;
    if (WiFi.status() == WL_CONNECTED) checkDDNS();
    lastCheck = millis();
  }
}
