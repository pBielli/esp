#include "Logger.h"

LogEntry logBuffer[LOG_BUFFER_SIZE];
int logHead = 0;
int logCount = 0;

void logBegin() {
  for (int i = 0; i < LOG_BUFFER_SIZE; i++) logBuffer[i].ms = 0;
}

void logAdd(unsigned long ms, String msg) {
  logBuffer[logHead].ms = ms;
  strncpy(logBuffer[logHead].msg, msg.c_str(), 63);
  logBuffer[logHead].msg[63] = '\0';
  logHead = (logHead + 1) % LOG_BUFFER_SIZE;
  if (logCount < LOG_BUFFER_SIZE) logCount++;
}

String logGet() {
  String out = "[";
  int start = (logHead - logCount + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
  for (int i = 0; i < logCount; i++) {
    int idx = (start + i) % LOG_BUFFER_SIZE;
    unsigned long ms = logBuffer[idx].ms;
    out += "{\"time\":" + String(ms) + ",\"msg\":\"" + String(logBuffer[idx].msg) + "\"}";
    if (i < logCount - 1) out += ",";
  }
  out += "]";
  return out;
}
