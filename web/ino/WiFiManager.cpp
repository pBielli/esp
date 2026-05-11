#include "WiFiManager.h"
#include "Config.h"
#include "Logger.h"
#include <ESP8266WiFi.h>

DNSServer dnsServer;
static bool apRunning = false;

void wifiSetup() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);

  int netIdx = networkListTryConnect();

  if (netIdx >= 0) {
    WifiNetwork net;
    if (networkListGet(netIdx, net)) {
      logPrint("WIFI", "Connected to: " + String(net.ssid) + " (" + WiFi.localIP().toString() + ")");
    } else {
      logPrint("WIFI", "Connected: " + WiFi.localIP().toString());
    }
  } else {
    logPrint("WIFI", "No saved network could be connected");
  }

  if (WiFi.status() != WL_CONNECTED && cfg.ap_fallback) {
    wifiAPStart();
  }
}

void wifiLoop() {
  if (apRunning) {
    dnsServer.processNextRequest();
  }

  static bool wasConnected = (WiFi.status() == WL_CONNECTED);
  bool nowConnected = (WiFi.status() == WL_CONNECTED);

  if (nowConnected && !wasConnected) {
    logPrint("WIFI", "Connected: " + WiFi.localIP().toString());
  }

  if (cfg.ap_fallback && !nowConnected && !apRunning) {
    wifiAPStart();
  }

  if (cfg.ap_fallback && nowConnected && apRunning) {
    // Keep AP running even when STA is connected (allows config access)
  }

  wasConnected = nowConnected;
}

bool wifiAPStart() {
  if (apRunning) return true;

  String apSsid = String(cfg.ap_ssid);
  if (apSsid.length() == 0) {
    apSsid = "ESP-" + WiFi.macAddress();
    apSsid.replace(":", "");
    apSsid = apSsid.substring(0, 16);
  }

  String apPass = String(cfg.ap_password);

  bool ok;
  if (apPass.length() > 0) {
    ok = WiFi.softAP(apSsid.c_str(), apPass.c_str());
  } else {
    ok = WiFi.softAP(apSsid.c_str());
  }

  if (!ok) {
    logPrint("WIFI", "AP start FAILED");
    return false;
  }

  delay(100);
  dnsServer.start(53, "*", WiFi.softAPIP());
  apRunning = true;
  logPrint("WIFI", "AP started: " + apSsid + " @ " + WiFi.softAPIP().toString());
  return true;
}

void wifiAPStop() {
  if (!apRunning) return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apRunning = false;
  logPrint("WIFI", "AP stopped");
}

bool wifiAPRunning() {
  return apRunning;
}

int wifiAPClientCount() {
  return WiFi.softAPgetStationNum();
}

String wifiAPSSID() {
  return WiFi.softAPSSID();
}

String wifiAPIP() {
  return WiFi.softAPIP().toString();
}
