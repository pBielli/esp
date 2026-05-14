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
#include "LED.h"
#include "Logger.h"
#include "NTP.h"
#include "GPIO.h"
#include "DDNS.h"
#include "WiFiManager.h"
#include "OTA.h"
#include "WebServer.h"

unsigned long lastCheck = 0;
unsigned long lastLedToggle = 0;
bool flag_firstRun = true;

void setup() {
  Serial.begin(115200);
  Serial.println("\n");
  Serial.print("Board: ");
  Serial.print(FIRMWARE_BOARD);
  Serial.print("  Project: ");
  Serial.print(FIRMWARE_PROJECT);
  Serial.print("  FW: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("Startup");
  Serial.print("Chip ID: 0x");
  Serial.println(ESP.getChipId(), HEX);
  Serial.print("Flash: ");
  Serial.print(ESP.getFlashChipRealSize() / 1024);
  Serial.println("KB");
  Serial.print("Free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  storageBegin();
  storageLoad();
  if (!storageInitialized()) {
    storageReset();
    networkListReset();
    logPrint("CONFIG", "reset to default");
  }
  networkListLoad();

  ledSetup();

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

  arduinoOtaSetup();
  setupRoutes();
  logPrint("SYS", String("Board: ") + FIRMWARE_BOARD + " Project: " + FIRMWARE_PROJECT + " FW: " + FIRMWARE_VERSION);
  logPrint("SYS", "System started");
  lastCheck = millis();
}

void loop() {
  ledLoop();
  server.handleClient();
  MDNS.update();
  wifiLoop();
  arduinoOtaLoop();
  otaLoop();

  if (millis() - lastLedToggle >= 1000) {
    lastLedToggle = millis();
    ledRunToggle();
  }

  static bool wifiWasConnected = false;
  if (WiFi.status() == WL_CONNECTED && !wifiWasConnected) {
    wifiWasConnected = true;
    ledWifiSet(true);
    logPrint("WIFI", "Connected: " + WiFi.localIP().toString());
    applyNetworkConfig();
    validateNetworkConfig();
  } else if (WiFi.status() != WL_CONNECTED) {
    wifiWasConnected = false;
    ledWifiSet(false);
  }

  if (millis() - lastCheck > (unsigned long)cfg.ddns_check_interval * 1000 || flag_firstRun) {
    if(flag_firstRun)
      flag_firstRun = false;
    if (WiFi.status() == WL_CONNECTED) checkDDNS();
    lastCheck = millis();
  }
}
