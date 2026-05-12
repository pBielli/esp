#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

void otaSetup();
void otaLoop();
bool otaCheckNow();
bool otaUpdateFromUrl(const String &url);
String otaLastError();
unsigned long otaLastCheckMs();

void arduinoOtaSetup();
void arduinoOtaLoop();

#endif
