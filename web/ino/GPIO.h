#ifndef GPIO_H
#define GPIO_H

#include <Arduino.h>

String gpioSet(int pin, String mode, String value);
String gpioRead(int pin);
String gpioInfo();
void ledOn();
void ledOff();
void ledBlink(int times);
String gpioBlink(int pin, int times, int ms = 200);
String analogReadPin();
String analogWritePin(int pin, int value);
String gpioPulse(int pin, int ms);
String gpioToggle(int pin);

#endif
