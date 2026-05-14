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

  // Direct update for binary URLs (skip metadata fetch entirely)
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
    lastError = "Check HTTP " + String(code) + " - check failed";
    logPrint("OTA", lastError);
    return false;
  }

  if (payload.length() == 0) {
    lastError = "Empty metadata response";
    logPrint("OTA", lastError);
    return false;
  }

  // Try JSON metadata first (ArduinoJson handles BOM/surrounding whitespace)
  {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      const char* version = doc["version"];
      const char* binUrl = doc["url"];
      if (binUrl && strlen(binUrl) > 0) {
        if (version && strcmp(version, FIRMWARE_VERSION) == 0) {
          logPrint("OTA", "Version matches: " + String(version));
          lastError = "Already up to date";
          return true;
        }
        effectiveUrl = String(binUrl);
        logPrint("OTA", "Updating from: " + effectiveUrl);
        return otaUpdateFromUrl(effectiveUrl);
      }
      lastError = "No firmware URL in JSON metadata";
      logPrint("OTA", lastError);
      return false;
    }
  }

  // Fallback: plain text URL response
  {
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

  ledOtaSet(true);

  HTTPClient http;
  http.setTimeout(30000);
  http.setUserAgent("ESP8266-OTA/1.0");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClientSecure httpsClient;
  WiFiClient plainClient;
  if (url.startsWith("https://")) {
    httpsClient.setInsecure();
    http.begin(httpsClient, url);
  } else {
    http.begin(plainClient, url);
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    lastError = "OTA: HTTP GET failed: " + String(code);
    logPrint("OTA", lastError);
    http.end();
    ledOtaSet(false);
    return false;
  }

  int totalSize = http.getSize();
  if (totalSize <= 0) {
    lastError = "OTA: Invalid size: " + String(totalSize);
    logPrint("OTA", lastError);
    http.end();
    ledOtaSet(false);
    return false;
  }

  logPrint("OTA", "Downloading " + String(totalSize) + " bytes");

  WiFiClient *stream = http.getStreamPtr();

  if (!Update.begin(totalSize)) {
    lastError = "OTA: Update.begin: " + String(Update.getError());
    logPrint("OTA", lastError);
    http.end();
    ledOtaSet(false);
    return false;
  }

  size_t written = Update.writeStream(*stream);
  if (written != (size_t)totalSize) {
    lastError = "OTA: Written " + String(written) + "/" + String(totalSize);
    logPrint("OTA", lastError);
    http.end();
    ledOtaSet(false);
    return false;
  }

  if (!Update.end()) {
    lastError = "OTA: Update.end: " + String(Update.getError());
    logPrint("OTA", lastError);
    http.end();
    ledOtaSet(false);
    return false;
  }

  http.end();
  ledOtaSet(false);

  logPrint("OTA", "Update OK - rebooting");
  delay(500);
  ESP.restart();
  return true;
}

String otaLastError() {
  return lastError;
}

unsigned long otaLastCheckMs() {
  return lastCheck;
}
