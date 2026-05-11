#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

extern ESP8266WebServer server;

String base64Decode(String input);
bool checkAuth();
void setupRoutes();

#endif
