#include "OTA.h"
#include "Config.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

static unsigned long lastCheck = 0;
static String lastError = "";

void otaSetup() {
  lastCheck = 0;
}

void otaLoop() {
  if (cfg.ota_check_interval <= 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastCheck < (unsigned long)cfg.ota_check_interval * 1000) return;
  otaCheckNow();
}

bool otaCheckNow() {
  if (WiFi.status() != WL_CONNECTED) {
    lastError = "WiFi not connected";
    return false;
  }

  lastCheck = millis();

  String url = String(cfg.ota_url);
  if (url.length() == 0) {
    lastError = "No OTA URL configured";
    return false;
  }

  logPrint("OTA", "Checking: " + url);

  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("ESP8266-OTA/1.0");
  WiFiClient client;
  http.begin(client, url);

  String payload;
  String effectiveUrl = url;
  WiFiClientSecure httpsClient;

  if (url.startsWith("https://")) {
    http.end();
    httpsClient.setInsecure();
    http.begin(httpsClient, url);
  }

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
    payload.trim();
  }
  http.end();

  if (code != HTTP_CODE_OK) {
    lastError = "HTTP " + String(code);
    logPrint("OTA", "Check failed: " + lastError);
    return false;
  }

  if (payload.length() == 0) {
    lastError = "Empty response";
    return false;
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
    // Plain URL response - treat as firmware URL directly
    effectiveUrl = payload;
  }

  logPrint("OTA", "Updating from: " + effectiveUrl);
  return otaUpdateFromUrl(effectiveUrl);
}

bool otaUpdateFromUrl(const String &url) {
  lastError = "";
  logPrint("OTA", "Starting OTA update: " + url);

  WiFiClient client;
  WiFiClientSecure httpsClient;
  t_httpUpdate_return ret;

  if (url.startsWith("https://")) {
    httpsClient.setInsecure();
    ret = ESPhttpUpdate.update(httpsClient, url, FIRMWARE_VERSION);
  } else {
    ret = ESPhttpUpdate.update(client, url, FIRMWARE_VERSION);
  }

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
