#include "Config.h"
#include <ESP8266WiFi.h>

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
  strncpy(cfg.wifi_ssid, "YOUR_SSID", sizeof(cfg.wifi_ssid) - 1);
  strncpy(cfg.wifi_password, "YOUR_PASSWORD", sizeof(cfg.wifi_password) - 1);
  strncpy(cfg.mdns_name, "ddns-checker", sizeof(cfg.mdns_name) - 1);
  strncpy(cfg.admin_password, "admin", sizeof(cfg.admin_password) - 1);
  strncpy(cfg.ddns_hostname, "YOUR_HOSTNAME.duckdns.org", sizeof(cfg.ddns_hostname) - 1);
  strncpy(cfg.ddns_token, "YOUR_TOKEN", sizeof(cfg.ddns_token) - 1);
  strncpy(cfg.ddns_domain, "YOUR_DOMAIN", sizeof(cfg.ddns_domain) - 1);
  cfg.led_pin = 2;
  cfg.tz_offset = 3600;
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
  } else if (cfg.use_custom_dns) {
    IPAddress dns1, dns2;
    bool ok1 = dns1.fromString(cfg.static_dns1);
    bool ok2 = dns2.fromString(cfg.static_dns2);
    if (ok1 || ok2) {
      WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    }
  }
}
