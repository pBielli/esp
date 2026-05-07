#include "Config.h"

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
  strncpy(cfg.wifi_ssid, "CasaBielli", 31);
  strncpy(cfg.wifi_password, "CasaBielli01", 63);
  strncpy(cfg.mdns_name, "ddns-checker", 31);
  strncpy(cfg.admin_password, "admin", 31);
  strncpy(cfg.ddns_hostname, "patb.duckdns.org", 63);
  strncpy(cfg.duckdns_token, "3894523a-d53d-433d-b661-83ac156319fa", 47);
  strncpy(cfg.duckdns_domain, "patb", 31);
  cfg.led_pin = 2;
  storageSave();
}

// void storageReset() {
//   strncpy(cfg.wifi_ssid, "YOUR_SSID", 31);
//   strncpy(cfg.wifi_password, "YOUR_PASSWORD", 63);
//   strncpy(cfg.mdns_name, "YOUR_MDNS_NAME", 31);
//   strncpy(cfg.admin_password, "YOUR_ADMIN_PASSWORD", 31);
//   strncpy(cfg.ddns_hostname, "YOUR_DUCKDNS_HOSTNAME", 63);
//   strncpy(cfg.duckdns_token, "YOUR_DUCKDNS_TOKEN", 47);
//   strncpy(cfg.duckdns_domain, "YOUR_DUCKDNS_DOMAIN", 31);
//   cfg.led_pin = 2;
//   storageSave();
// }


bool storageInitialized() {
  return strcmp(cfg.magic, "DDC") == 0;
}
