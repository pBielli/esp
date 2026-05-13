#include "LED.h"
#include "Config.h"
#include "GPIO.h"

static unsigned long webPulseEnd = 0;
static bool webPulseActive = false;

static void ledWrite(int pin, bool on) {
  if (pin < 0) return;
  pinMode(pin, OUTPUT);
  if (cfg.gpio_invert) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

static bool ledRead(int pin) {
  if (pin < 0) return false;
  int val = digitalRead(pin);
  if (cfg.gpio_invert) return val == LOW;
  return val == HIGH;
}

void ledSetup() {
  if (cfg.led_run_pin >= 0) {
    pinMode(cfg.led_run_pin, OUTPUT);
    ledWrite(cfg.led_run_pin, false);
  }
  if (cfg.led_wifi_pin >= 0) {
    pinMode(cfg.led_wifi_pin, OUTPUT);
    ledWrite(cfg.led_wifi_pin, false);
  }
  if (cfg.led_ota_pin >= 0) {
    pinMode(cfg.led_ota_pin, OUTPUT);
    ledWrite(cfg.led_ota_pin, false);
  }
  if (cfg.led_web_pin >= 0) {
    pinMode(cfg.led_web_pin, OUTPUT);
    ledWrite(cfg.led_web_pin, false);
  }
}

void ledRunToggle() {
  if (cfg.led_run_pin < 0) return;
  ledWrite(cfg.led_run_pin, !ledRead(cfg.led_run_pin));
}

void ledWifiSet(bool connected) {
  if (cfg.led_wifi_pin < 0) return;
  ledWrite(cfg.led_wifi_pin, connected);
}

void ledOtaSet(bool active) {
  if (cfg.led_ota_pin < 0) return;
  ledWrite(cfg.led_ota_pin, active);
}

void ledWebPulse() {
  if (cfg.led_web_pin < 0) return;
  ledWrite(cfg.led_web_pin, true);
  webPulseEnd = millis() + 50;
  webPulseActive = true;
}

void ledLoop() {
  if (webPulseActive && millis() >= webPulseEnd) {
    webPulseActive = false;
    ledWrite(cfg.led_web_pin, false);
  }
}
