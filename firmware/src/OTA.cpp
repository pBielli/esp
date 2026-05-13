#include "OTA.h"
#include "Config.h"
#include "LED.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

static unsigned long lastCheck = 0;
static String lastError = "";

// ── ArduinoOTA (push OTA via espota) ──────────────────────────
void arduinoOtaSetup() {
  ArduinoOTA.setHostname(cfg.mdns_name);
  ArduinoOTA.setPassword(cfg.admin_password);

  ArduinoOTA.onStart([]() {
    logPrint("OTA", "ArduinoOTA: start");
  });
  ArduinoOTA.onEnd([]() {
    logPrint("OTA", "ArduinoOTA: end");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static int lastPct = -1;
    int pct = progress / (total / 100);
    if (pct != lastPct) {
      lastPct = pct;
      logPrint("OTA", "ArduinoOTA: " + String(pct) + "%");
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    String msg = "ArduinoOTA: error ";
    if (error == OTA_AUTH_ERROR) msg += "auth";
    else if (error == OTA_BEGIN_ERROR) msg += "begin";
    else if (error == OTA_CONNECT_ERROR) msg += "connect";
    else if (error == OTA_RECEIVE_ERROR) msg += "receive";
    else if (error == OTA_END_ERROR) msg += "end";
    logPrint("OTA", msg);
  });

  ArduinoOTA.begin();
  logPrint("OTA", "ArduinoOTA ready");
}

void arduinoOtaLoop() {
  ArduinoOTA.handle();
}

void otaSetup() {
  lastCheck = 0;
}

void otaLoop() {
  if (cfg.ota_check_interval <= 0) return;
  if (!WiFi.isConnected()) return;
  if (millis() - lastCheck < (unsigned long)cfg.ota_check_interval * 1000) return;
  otaCheckNow();
}

bool otaCheckNow() {
  if (WiFi.localIP() == IPAddress(0, 0, 0, 0) || WiFi.SSID().length() == 0) {
    lastError = "WiFi not connected";
    logPrint("OTA", "Check skipped: WiFi not connected (" + String(WiFi.status()) + ")");
    return false;
  }

  lastCheck = millis();

  String url = String(cfg.ota_url);
  if (url.length() == 0) {
    url = otaDefaultUrl();
    logPrint("OTA", "Using default URL: " + url);
  }

  logPrint("OTA", "Checking: " + url);

  String effectiveUrl = url;

  // If URL ends with .bin, skip HTTP check and try update directly
  if (url.endsWith(".bin")) {
    logPrint("OTA", "Direct binary URL, updating");
    return otaUpdateFromUrl(url);
  }

  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("ESP8266-OTA/1.0");

  WiFiClient client;
  WiFiClientSecure httpsClient;
  bool isHttps = url.startsWith("https://");

  if (isHttps) {
    httpsClient.setInsecure();
    http.begin(httpsClient, url);
  } else {
    http.begin(client, url);
  }

  String payload;

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
    payload.trim();
  }
  http.end();

  if (code != HTTP_CODE_OK) {
    logPrint("OTA", "Check HTTP " + String(code) + " - trying direct update");
    return otaUpdateFromUrl(url);
  }

  if (payload.length() == 0) {
    logPrint("OTA", "Empty response - trying direct update");
    return otaUpdateFromUrl(url);
  }

  // Check if response is JSON metadata
  if (payload.startsWith("{")) {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      lastError = "Invalid JSON metadata";
      return false;
    }
    const char* version = doc["version"];
    const char* binUrl = doc["url"];
    if (!binUrl || strlen(binUrl) == 0) {
      lastError = "No firmware URL in metadata";
      return false;
    }
    if (version && strcmp(version, FIRMWARE_VERSION) == 0) {
      logPrint("OTA", "Version matches: " + String(version));
      lastError = "Already up to date";
      return true;
    }
    effectiveUrl = String(binUrl);
  } else {
    // Plain text URL response
    String trimmed = payload;
    trimmed.trim();
    if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
      effectiveUrl = trimmed;
    } else {
      effectiveUrl = url;
    }
  }

  logPrint("OTA", "Updating from: " + effectiveUrl);
  return otaUpdateFromUrl(effectiveUrl);
}

bool otaUpdateFromUrl(const String &url) {
  lastError = "";
  logPrint("OTA", "Starting OTA update: " + url);

  ledOtaSet(true);

  WiFiClientSecure client;
  client.setInsecure();

  t_httpUpdate_return ret = ESPhttpUpdate.update(client, url, FIRMWARE_VERSION);

  ledOtaSet(false);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      lastError = "Update failed: " + ESPhttpUpdate.getLastErrorString();
      logPrint("OTA", lastError);
      return false;
    case HTTP_UPDATE_NO_UPDATES:
      lastError = "No update available";
      logPrint("OTA", "No update available");
      return false;
    case HTTP_UPDATE_OK:
      logPrint("OTA", "Update OK - rebooting");
      return true;
  }
  return false;
}

String otaLastError() {
  return lastError;
}

unsigned long otaLastCheckMs() {
  return lastCheck;
}
