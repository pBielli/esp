#ifndef LED_H
#define LED_H

#include <Arduino.h>

void ledSetup();
void ledRunToggle();
void ledWifiSet(bool connected);
void ledOtaSet(bool active);
void ledWebPulse();
void ledLoop();

#endif
