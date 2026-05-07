#ifndef CONFIG_H
#define CONFIG_H

#include <EEPROM.h>
#include <Arduino.h>

#define EEPROM_SIZE 512
#define MAGIC "DDC"

struct Config {
  char wifi_ssid[32];
  char wifi_password[64];
  char mdns_name[32];
  char admin_password[32];
  char ddns_hostname[64];
  char duckdns_token[48];
  char duckdns_domain[32];
  int led_pin;
  char magic[4];
};

extern Config cfg;

void storageBegin();
void storageLoad();
void storageSave();
void storageReset();
bool storageInitialized();

#endif
