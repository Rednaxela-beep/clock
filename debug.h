// debug.h
#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "arrow.h"  // enum ArrowState и extern arrowState уже здесь
void debugLogf(const char* fmt, ...);  // "Обёрточный логгер" - единая точка вывода логов для замены Serial.println

// Функция для получения имени состояния
inline const char* stateName(ArrowState state) {
  switch (state) {
    case IDLE: return "IDLE";
    case MOVING: return "MOVING";
    default: return "UNKNOWN";
  }
}

void debugSerialLoop();  // Обработка команд из Serial

void debugDump(DateTime now, bool microSwitchState);  // Прототип отладочного дампа

void webMonitorBegin();  // Прототип Веб Монитора
void webMonitorLoop();
