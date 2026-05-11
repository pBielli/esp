#include "NTP.h"
#include "Config.h"
#include "Logger.h"

unsigned long lastNtpSync = 0;
const unsigned long ntpSyncInterval = 86400000;

void ntpBegin() {
  configTime(cfg.tz_offset, 0, cfg.ntp_server);
  delay(500);
  logPrint("NTP", "Syncing from " + String(cfg.ntp_server));
  for (int i = 0; i < 10; i++) {
    time_t now = time(nullptr);
    if (now > 100000) {
      lastNtpSync = millis();
      logPrint("NTP", "Sync OK");
      return;
    }
    delay(500);
  }
  logPrint("NTP", "Sync FAILED");
}

String ntpGetTime() {
  if (millis() - lastNtpSync > ntpSyncInterval) ntpBegin();
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char buf[32];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
          t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

time_t ntpGetEpoch() {
  if (millis() - lastNtpSync > ntpSyncInterval) ntpBegin();
  return time(nullptr);
}
