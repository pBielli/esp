#ifndef NTP_H
#define NTP_H

#include <Arduino.h>
#include <time.h>

extern unsigned long lastNtpSync;
extern const unsigned long ntpSyncInterval;

void ntpBegin();
String ntpGetTime();
time_t ntpGetEpoch();

#endif
