#include "OTA.h"
#include "Config.h"
#include "LED.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

static unsigned long lastCheck = 0;
static String lastError = "";

// Compare semver "major.minor.patch" — returns true if online > current
static bool isNewerVersion(const char* current, const char* online) {
  int cMaj = 0, cMin = 0, cPat = 0;
  int oMaj = 0, oMin = 0, oPat = 0;
  sscanf(current, "%d.%d.%d", &cMaj, &cMin, &cPat);
  sscanf(online,   "%d.%d.%d", &oMaj, &oMin, &oPat);
  if (oMaj != cMaj) return oMaj > cMaj;
  if (oMin != cMin) return oMin > cMin;
  return oPat > cPat;
}

// Resolve hostname, preferring IPv4
static bool resolveIPv4(const char* host, IPAddress& result) {
  if (!WiFi.hostByName(host, result, 10000)) {
    logPrint("OTA", String("DNS fail: ") + host);
    return false;
  }
  logPrint("OTA", String("DNS ") + host + " -> " + result.toString());
  return true;
}

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
  http.setTimeout(20000);
  http.setUserAgent("ESP8266-OTA/1.0");

  WiFiClient client;
  WiFiClientSecure httpsClient;
  bool isHttps = url.startsWith("https://");

  String useUrl = url;
  if (isHttps) {
    httpsClient.setBufferSizes(2048, 512);
    httpsClient.setInsecure();
    http.begin(httpsClient, useUrl);
  } else {
    http.begin(client, useUrl);
  }

  String payload;

  int code = http.GET();

  // Follow HTTP->HTTPS redirect (e.g. GitHub Pages)
  if ((code == HTTP_CODE_MOVED_PERMANENTLY || code == HTTP_CODE_FOUND) && !isHttps) {
    String redirectUrl = http.header("Location");
    logPrint("OTA", "Redirect to: " + redirectUrl);
    http.end();
    if (redirectUrl.startsWith("https://")) {
      httpsClient.setBufferSizes(512, 512);
      httpsClient.setInsecure();
      http.begin(httpsClient, redirectUrl);
      code = http.GET();
    } else if (redirectUrl.length() > 0) {
      http.begin(client, redirectUrl);
      code = http.GET();
    }
  }

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

  // Strip UTF-8 BOM (0xEF 0xBB 0xBF) if present
  if (payload.length() >= 3 && (uint8_t)payload[0] == 0xEF && (uint8_t)payload[1] == 0xBB && (uint8_t)payload[2] == 0xBF) {
    payload = payload.substring(3);
    logPrint("OTA", "Stripped BOM");
  }

  // Try JSON metadata first
  {
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      const char* version = doc["version"];
      const char* binUrl = doc["url"];
      if (binUrl && strlen(binUrl) > 0) {
        if (version && !isNewerVersion(FIRMWARE_VERSION, version)) {
          logPrint("OTA", "Up to date (" + String(version) + ")");
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
  logPrint("OTA", "Starting OTA update: " + url);

  if (url.startsWith("https://")) {
    WiFiClientSecure client;
    client.setBufferSizes(2048, 512);
    client.setInsecure();

    // Pre-resolve hostname to IPv4 (avoid IPv6 DNS issues)
    String useUrl = url;
    int hostStart = 8;
    int hostEnd = url.indexOf('/', hostStart);
    if (hostEnd > hostStart) {
      String origHost = url.substring(hostStart, hostEnd);
      String path = url.substring(hostEnd);
      IPAddress ip4;
      if (resolveIPv4(origHost.c_str(), ip4)) {
        useUrl = "https://" + ip4.toString() + path;
        logPrint("OTA", "Resolved " + origHost + " -> " + ip4.toString());
      }
    }

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, useUrl, FIRMWARE_VERSION);
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
  } else {
    WiFiClient client;
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
  }
  return false;
}

String otaLastError() {
  return lastError;
}

unsigned long otaLastCheckMs() {
  return lastCheck;
}
