#include "Config.h"
#include "GPIO.h"
#include "Logger.h"
#include "user_config.h"
#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>

Config cfg;

static NetworkList nl;

void storageBegin() {
  EEPROM.begin(EEPROM_SIZE);
}

void storageLoad() {
  EEPROM.get(0, cfg);
}

void storageSave() {
  cfg.magic[0] = 'D';
  cfg.magic[1] = 'D';
  cfg.magic[2] = 'C';
  cfg.magic[3] = '\0';
  EEPROM.put(0, cfg);
  EEPROM.commit();
  logPrint("CONFIG", "Saved to EEPROM");
}

void storageReset() {
  logPrint("CONFIG", "Resetting to factory defaults");
  strncpy(cfg.wifi_ssid,       DEFAULT_WIFI_SSID,      sizeof(cfg.wifi_ssid) - 1);
  strncpy(cfg.wifi_password,   DEFAULT_WIFI_PASSWORD,  sizeof(cfg.wifi_password) - 1);
  strncpy(cfg.mdns_name,       DEFAULT_MDNS_NAME,      sizeof(cfg.mdns_name) - 1);
  strncpy(cfg.admin_password,  DEFAULT_ADMIN_PASSWORD, sizeof(cfg.admin_password) - 1);
  strncpy(cfg.ddns_hostname,   DEFAULT_DDNS_HOSTNAME,  sizeof(cfg.ddns_hostname) - 1);
  strncpy(cfg.ddns_token,      DEFAULT_DDNS_TOKEN,     sizeof(cfg.ddns_token) - 1);
  strncpy(cfg.ddns_domain,     DEFAULT_DDNS_DOMAIN,    sizeof(cfg.ddns_domain) - 1);
  cfg.led_pin              = DEFAULT_LED_PIN;
  cfg.tz_offset            = DEFAULT_TZ_OFFSET;
  strncpy(cfg.ntp_server,    DEFAULT_NTP_SERVER,       sizeof(cfg.ntp_server) - 1);
  cfg.gpio_invert          = DEFAULT_GPIO_INVERT;
  cfg.use_static_ip        = 0;
  strncpy(cfg.static_ip,      "", sizeof(cfg.static_ip) - 1);
  strncpy(cfg.static_gateway, "", sizeof(cfg.static_gateway) - 1);
  strncpy(cfg.static_subnet,  "", sizeof(cfg.static_subnet) - 1);
  strncpy(cfg.static_dns1,    "", sizeof(cfg.static_dns1) - 1);
  strncpy(cfg.static_dns2,    "", sizeof(cfg.static_dns2) - 1);
  cfg.pwm_pin              = DEFAULT_PWM_PIN;
  cfg.use_custom_dns       = 0;
  cfg.ddns_check_interval  = DEFAULT_DDNS_INTERVAL;
  strncpy(cfg.ddns_upd_url,    "https://www.duckdns.org/update?domains=$domain&token=$token&ip=$ip", sizeof(cfg.ddns_upd_url) - 1);
  strncpy(cfg.public_ip_urls,  "api.ipify.org,api.my-ip.io/ip",  sizeof(cfg.public_ip_urls) - 1);
  strncpy(cfg.ap_ssid,         DEFAULT_AP_SSID,        sizeof(cfg.ap_ssid) - 1);
  strncpy(cfg.ap_password,     DEFAULT_AP_PASSWORD,    sizeof(cfg.ap_password) - 1);
  cfg.ap_fallback          = 1;
  strncpy(cfg.ota_url,         DEFAULT_OTA_URL,        sizeof(cfg.ota_url) - 1);
  cfg.ota_check_interval   = DEFAULT_OTA_INTERVAL;
  cfg.wifi_retry_count     = DEFAULT_WIFI_RETRY;
  storageSave();
}

bool storageInitialized() {
  return strcmp(cfg.magic, "DDC") == 0;
}

void applyNetworkConfig() {
  if (cfg.use_static_ip) {
    IPAddress ip, gw, subnet, dns1, dns2;
    ip.fromString(cfg.static_ip);
    gw.fromString(cfg.static_gateway);
    subnet.fromString(cfg.static_subnet);
    dns1.fromString(cfg.static_dns1);
    dns2.fromString(cfg.static_dns2);
    WiFi.config(ip, gw, subnet, dns1, dns2);
  } else if (cfg.use_custom_dns && WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    IPAddress gw = WiFi.gatewayIP();
    IPAddress sn = WiFi.subnetMask();
    IPAddress dns1, dns2;
    dns1.fromString(cfg.static_dns1);
    dns2.fromString(cfg.static_dns2);
    WiFi.config(ip, gw, sn, dns1, dns2);
    logAdd(millis(), "Custom DNS applied: " + ip.toString() + " / " + String(cfg.static_dns1) + ", " + String(cfg.static_dns2));
  }
}

bool validateNetworkConfig() {
  if (!cfg.use_static_ip && !cfg.use_custom_dns) return true;
  if (WiFi.status() != WL_CONNECTED) return true;

  IPAddress gw = WiFi.gatewayIP();
  IPAddress testIp(8, 8, 8, 8);

  bool gwOk  = Ping.ping(gw, 2);
  bool extOk = Ping.ping(testIp, 2);

  if (!gwOk && !extOk) {
    logPrint("NET", "Validation FAILED: gw=" + String(gwOk ? "OK" : "FAIL") + " 8.8.8.8=" + String(extOk ? "OK" : "FAIL") + " - switching to DHCP");
    cfg.use_static_ip  = 0;
    cfg.use_custom_dns = 0;
    storageSave();
    WiFi.config(0u, 0u, 0u);
    ledBlink(8);
    return false;
  }
  return true;
}

// ── NetworkList ─────────────────────────────────────────────

void networkListLoad() {
  EEPROM.get(NETWORK_LIST_OFFSET, nl);
  if (strcmp(nl.magic, "WNET") != 0) {
    nl.count = 1;
    strncpy(nl.networks[0].ssid,     cfg.wifi_ssid,     32);
    nl.networks[0].ssid[32] = '\0';
    strncpy(nl.networks[0].password, cfg.wifi_password, 64);
    nl.networks[0].password[64] = '\0';
    strncpy(nl.magic, "WNET", 4);
    nl.magic[4] = '\0';
    networkListSave();
  }
}

void networkListReset() {
  nl.count = 0;
  strncpy(nl.magic, "xx", 2);
  networkListSave();
}

void networkListSave() {
  EEPROM.put(NETWORK_LIST_OFFSET, nl);
  EEPROM.commit();
}

int networkListTryConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int retries = cfg.wifi_retry_count > 0 ? cfg.wifi_retry_count : 1;
  for (int i = 0; i < nl.count; i++) {
    logPrint("WIFI", "Trying: " + String(nl.networks[i].ssid));
    for (int t = 0; t < retries; t++) {
      if (t > 0) logPrint("WIFI", "Retry " + String(t + 1) + "/" + String(retries) + " for " + String(nl.networks[i].ssid));
      WiFi.begin(nl.networks[i].ssid, nl.networks[i].password);
      for (int w = 0; w < 50; w++) {
        if (WiFi.status() == WL_CONNECTED) {
          strncpy(cfg.wifi_ssid,     nl.networks[i].ssid,     sizeof(cfg.wifi_ssid) - 1);
          strncpy(cfg.wifi_password, nl.networks[i].password, sizeof(cfg.wifi_password) - 1);
          return i;
        }
        delay(200);
      }
    }
    WiFi.disconnect();
    delay(50);
  }
  return -1;
}

int networkListCount()                     { return nl.count; }

bool networkListGet(int idx, WifiNetwork &net) {
  if (idx < 0 || idx >= nl.count) return false;
  net = nl.networks[idx];
  return true;
}

bool networkListAdd(const char *ssid, const char *password) {
  if (nl.count >= MAX_NETWORKS) return false;
  strncpy(nl.networks[nl.count].ssid,     ssid,     32);
  nl.networks[nl.count].ssid[32] = '\0';
  strncpy(nl.networks[nl.count].password, password, 64);
  nl.networks[nl.count].password[64] = '\0';
  nl.count++;
  networkListSave();
  return true;
}

bool networkListRemove(int idx) {
  if (idx < 0 || idx >= nl.count) return false;
  for (int i = idx; i < nl.count - 1; i++) nl.networks[i] = nl.networks[i + 1];
  nl.count--;
  networkListSave();
  return true;
}

bool networkListMove(int from, int to) {
  if (from < 0 || from >= nl.count || to < 0 || to >= nl.count) return false;
  if (from == to) return true;
  WifiNetwork tmp = nl.networks[from];
  if (from < to) {
    for (int i = from; i < to; i++) nl.networks[i] = nl.networks[i + 1];
  } else {
    for (int i = from; i > to; i--) nl.networks[i] = nl.networks[i - 1];
  }
  nl.networks[to] = tmp;
  networkListSave();
  return true;
}
