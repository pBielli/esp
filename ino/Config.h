#ifndef CONFIG_H
#define CONFIG_H

#include <EEPROM.h>
#include <Arduino.h>

#define EEPROM_SIZE 2048
#define MAGIC "DDC"
#define CORS_ORIGIN "https://pbielli.github.io"
#define FIRMWARE_VERSION "1.0.0"

struct Config {
  char wifi_ssid[32];
  char wifi_password[64];
  char mdns_name[32];
  char admin_password[32];
  char ddns_hostname[64];
  char ddns_token[48];
  char ddns_domain[32];
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
  int use_custom_dns;
  int ddns_check_interval;
  char ddns_upd_url[256];
  char public_ip_urls[512];
  char magic[4];
};

extern Config cfg;

void storageBegin();
void storageLoad();
void storageSave();
void storageReset();
bool storageInitialized();
void applyNetworkConfig();
bool validateNetworkConfig();

#endif
