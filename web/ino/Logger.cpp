#include "Logger.h"

LogEntry logBuffer[LOG_BUFFER_SIZE];
int logHead = 0;
int logCount = 0;

void logAdd(unsigned long ms, String msg) {
  logBuffer[logHead].ms = ms;
  strncpy(logBuffer[logHead].msg, msg.c_str(), 63);
  logBuffer[logHead].msg[63] = '\0';
  logHead = (logHead + 1) % LOG_BUFFER_SIZE;
  if (logCount < LOG_BUFFER_SIZE) logCount++;
}

void logPrint(const String& tag, const String& msg) {
  String line = "[" + tag + "] " + msg;
  if (line.length() > 128) line = line.substring(0, 128);
  Serial.println(line);
  logAdd(millis(), line);
}

String escapeJson(const String& s) {
  String out = "";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c;
    }
  }
  return out;
}

String logGet() {
  String out = "[";
  int start = (logHead - logCount + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
  for (int i = 0; i < logCount; i++) {
    int idx = (start + i) % LOG_BUFFER_SIZE;
    unsigned long ms = logBuffer[idx].ms;
    out += "{\"time\":" + String(ms) + ",\"msg\":\"" + escapeJson(String(logBuffer[idx].msg)) + "\"}";
    if (i < logCount - 1) out += ",";
  }
  out += "]";
  return out;
}
