#include "Config.h"
#include "Logger.h"
#include "GPIO.h"
#include <ESP8266WiFi.h>
#include <ESPping.h>

Config cfg;

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
}

void storageReset() {
  strncpy(cfg.wifi_ssid, "CasaBielli", sizeof(cfg.wifi_ssid) - 1);
  strncpy(cfg.wifi_password, "CasaBielli01", sizeof(cfg.wifi_password) - 1);
  strncpy(cfg.mdns_name, "ddns-checker", sizeof(cfg.mdns_name) - 1);
  strncpy(cfg.admin_password, "admin", sizeof(cfg.admin_password) - 1);
  strncpy(cfg.ddns_hostname, "patbee.duckdns.org", sizeof(cfg.ddns_hostname) - 1);
  strncpy(cfg.ddns_token, "3894523a-d53d-433d-b661-83ac156319fa", sizeof(cfg.ddns_token) - 1);
  strncpy(cfg.ddns_domain, "patbee", sizeof(cfg.ddns_domain) - 1);
  cfg.led_pin = 2;
  cfg.tz_offset = 7200;
  strncpy(cfg.ntp_server, "pool.ntp.org", sizeof(cfg.ntp_server) - 1);
  cfg.led_invert = 0;
  cfg.use_static_ip = 0;
  strncpy(cfg.static_ip, "192.168.1.100", sizeof(cfg.static_ip) - 1);
  strncpy(cfg.static_gateway, "192.168.1.1", sizeof(cfg.static_gateway) - 1);
  strncpy(cfg.static_subnet, "255.255.255.0", sizeof(cfg.static_subnet) - 1);
  strncpy(cfg.static_dns1, "8.8.8.8", sizeof(cfg.static_dns1) - 1);
  strncpy(cfg.static_dns2, "8.8.4.4", sizeof(cfg.static_dns2) - 1);
  cfg.pwm_pin = 5;
  cfg.use_custom_dns = 0;
  cfg.ddns_check_interval = 300;
  strncpy(cfg.ddns_upd_url, "https://www.duckdns.org/update?domains=$domain&token=$token&ip=$ip", sizeof(cfg.ddns_upd_url) - 1);
  strncpy(cfg.public_ip_urls, "https://api.ipify.org,https://api.my-ip.io/ip,https://checkip.amazonaws.com,https://icanhazip.com", sizeof(cfg.public_ip_urls) - 1);
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
    String ipStr = WiFi.localIP().toString();
    String gwStr = WiFi.gatewayIP().toString();
    String subnetStr = WiFi.subnetMask().toString();
    strncpy(cfg.static_ip, ipStr.c_str(), sizeof(cfg.static_ip) - 1);
    strncpy(cfg.static_gateway, gwStr.c_str(), sizeof(cfg.static_gateway) - 1);
    strncpy(cfg.static_subnet, subnetStr.c_str(), sizeof(cfg.static_subnet) - 1);
    storageSave();
    IPAddress ip, gw, subnet, dns1, dns2;
    ip.fromString(cfg.static_ip);
    gw.fromString(cfg.static_gateway);
    subnet.fromString(cfg.static_subnet);
    dns1.fromString(cfg.static_dns1);
    dns2.fromString(cfg.static_dns2);
    WiFi.config(ip, gw, subnet, dns1, dns2);
    logAdd(millis(), "Custom DNS applied: " + String(cfg.static_ip) + " / " + String(cfg.static_dns1) + ", " + String(cfg.static_dns2));
  }
}

bool validateNetworkConfig() {
  if (!cfg.use_static_ip && !cfg.use_custom_dns) return true;
  if (WiFi.status() != WL_CONNECTED) return true;

  IPAddress gw = WiFi.gatewayIP();
  IPAddress testIp(8, 8, 8, 8);

  bool gwOk = Ping.ping(gw, 2);
  bool extOk = Ping.ping(testIp, 2);

  if (!gwOk && !extOk) {
    Serial.println("Network validation FAILED: gw=" + String(gwOk ? "OK" : "FAIL") + " 8.8.8.8=" + String(extOk ? "OK" : "FAIL") + " - switching to DHCP");
    logAdd(millis(), "Network validation FAILED - switching to DHCP");
    cfg.use_static_ip = 0;
    cfg.use_custom_dns = 0;
    storageSave();
    WiFi.config(0u, 0u, 0u);
    ledBlink(8);
    return false;
  }
  return true;
}
