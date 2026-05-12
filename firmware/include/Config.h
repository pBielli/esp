#ifndef CONFIG_H
#define CONFIG_H

#include <EEPROM.h>
#include <Arduino.h>

#define EEPROM_SIZE 2048
#define MAGIC "DDC"
#define CORS_ORIGIN "https://pbielli.github.io"
#define FIRMWARE_VERSION "1.0.2"

#define MAX_NETWORKS 4
#define NETWORK_LIST_OFFSET 1536

struct WifiNetwork {
  char ssid[33];
  char password[65];
};

struct NetworkList {
  char magic[5];
  int count;
  WifiNetwork networks[MAX_NETWORKS];
};

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
  int gpio_invert;
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
  char ap_ssid[32];
  char ap_password[32];
  int ap_fallback;
  char ota_url[256];
  int ota_check_interval;
  int wifi_retry_count;
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

void networkListLoad();
void networkListSave();
void networkListReset();
int networkListTryConnect();
int networkListCount();
bool networkListGet(int idx, WifiNetwork &net);
bool networkListAdd(const char *ssid, const char *password);
bool networkListRemove(int idx);
bool networkListMove(int from, int to);

#endif
