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
#include "WiFiManager.h"
#include "OTA.h"
#include "WebServer.h"

unsigned long lastCheck = 0;
bool flag_firstRun = true;

void setup() {
  Serial.begin(115200);
  Serial.println("\n");

  storageBegin();
  storageLoad();
  networkListLoad();
// storageReset();
  if (!storageInitialized()) {
    storageReset();
    networkListSave();
    logPrint("CONFIG", "reset to default");
  }

  pinMode(cfg.led_pin, OUTPUT);
  digitalWrite(cfg.led_pin, cfg.gpio_invert ? LOW : HIGH);

  applyNetworkConfig();
  wifiSetup();
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
  MDNS.update();
  wifiLoop();
  otaLoop();

  static bool wifiWasConnected = false;
  if (WiFi.status() == WL_CONNECTED && !wifiWasConnected) {
    wifiWasConnected = true;
    logPrint("WIFI", "Connected: " + WiFi.localIP().toString());
    applyNetworkConfig();
    validateNetworkConfig();
  } else if (WiFi.status() != WL_CONNECTED) {
    wifiWasConnected = false;
  }

  if (millis() - lastCheck > (unsigned long)cfg.ddns_check_interval * 1000 || flag_firstRun) {
    if(flag_firstRun)
      flag_firstRun = false;
    if (WiFi.status() == WL_CONNECTED) checkDDNS();
    lastCheck = millis();
  }
}
