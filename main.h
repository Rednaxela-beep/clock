//main.h Важные объявления
#pragma once
// #include <Arduino.h>
#include <RTClib.h>   // для DateTime
#include <ESP32Servo.h>     // чтобы Servo был известен
#include <AccelStepper.h>   // чтобы AccelStepper был известен

#include "config.h"
#include "arrow.h"
#include "debug.h"
// #include "wi-fi.h"
#include "chimes.h"         // чтобы chimesetup() был виден

// Глобальные объекты
extern Servo sg90;
extern RTC_DS3231 rtc;
extern AccelStepper stepper;
extern bool syncedThisHour;
bool microSwRaw();

// Прототипы "главных" функций
void setupMain();
void loopMain();

void connectToWiFi(); // Подключение к Wi‑Fi

void webMonitorBegin(); // Web Monitor
void webMonitorLoop(); 