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
  int tz_offset;
  char ntp_server[64];
  int led_invert;
  int use_static_ip;
  char static_ip[16];
  char static_gateway[16];
  char static_subnet[16];
  char static_dns1[16];
  char static_dns2[16];
  int pwm_pin;
  char magic[4];
};

extern Config cfg;

void storageBegin();
void storageLoad();
void storageSave();
void storageReset();
bool storageInitialized();

#endif
