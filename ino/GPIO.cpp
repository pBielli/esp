#include "GPIO.h"
#include "Config.h"

String gpioSet(int pin, String mode, String value) {
  if (pin < 0 || pin > 16) return "{\"error\":\"Invalid pin\"}";
  if (mode == "input") {
    pinMode(pin, INPUT);
    return "{\"pin\":" + String(pin) + ",\"mode\":\"input\"}";
  } else if (mode == "output") {
    pinMode(pin, OUTPUT);
    if (value == "1" || value == "HIGH") digitalWrite(pin, HIGH);
    else if (value == "0" || value == "LOW") digitalWrite(pin, LOW);
    return "{\"pin\":" + String(pin) + ",\"mode\":\"output\",\"value\":" + String(digitalRead(pin)) + "}";
  }
  return "{\"error\":\"Invalid mode\"}";
}

String gpioRead(int pin) {
  if (pin < 0 || pin > 16) return "{\"error\":\"Invalid pin\"}";
  int val = digitalRead(pin);
  return "{\"pin\":" + String(pin) + ",\"value\":" + String(val) + "}";
}

String gpioInfo() {
  String out = "{\"pins\":[";
  for (int i = 0; i <= 16; i++) {
    out += "{\"pin\":" + String(i);
    if (i == 0 || i == 2 || i == 4 || i == 5 || i == 12 || i == 13 || i == 14 || i == 15 || i == 16) {
      out += ",\"available\":true}";
    } else {
      out += ",\"available\":false}";
    }
    if (i < 16) out += ",";
  }
  out += "]}";
  return out;
}

void ledOn() {
  digitalWrite(cfg.led_pin, cfg.led_invert ? HIGH : LOW);
}

void ledOff() {
  digitalWrite(cfg.led_pin, cfg.led_invert ? LOW : HIGH);
}

String analogReadPin() {
  int val = analogRead(A0);
  float voltage = val * (3.3f / 1024.0f);
  int mapped = map(val, 0, 1023, 0, 100);
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"pin\":\"A0\",\"raw\":%d,\"voltage\":%.2f,\"value\":%d}", val, voltage, mapped);
  return String(buf);
}

String analogWritePin(int pin, int value) {
  if (pin < 0 || pin > 16) return "{\"error\":\"Invalid pin\"}";
  if (value < 0 || value > 100) return "{\"error\":\"Value must be 0-100\"}";
  pinMode(pin, OUTPUT);
  int pwmVal = map(value, 0, 100, 0, 1023);
  analogWrite(pin, pwmVal);
  char buf[96];
  snprintf(buf, sizeof(buf), "{\"pin\":%d,\"value\":%d,\"pwm\":%d}", pin, value, pwmVal);
  return String(buf);
}

void ledBlink(int times) {
  for (int i = 0; i < times; i++) {
    ledOn();
    delay(200);
    ledOff();
    delay(200);
  }
  ledOff();
}
