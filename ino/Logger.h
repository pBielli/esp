#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

#define LOG_BUFFER_SIZE 20

struct LogEntry {
  unsigned long ms;
  char msg[64];
};

void logBegin();
void logAdd(unsigned long ms, String msg);
void logPrint(const String& tag, const String& msg);
String logGet();

#endif
