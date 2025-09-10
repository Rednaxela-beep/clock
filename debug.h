#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "arrow.h" // enum ArrowState и extern arrowState уже здесь

// Функция для получения имени состояния
inline const char* stateName(ArrowState state) {
    switch (state) {
        case IDLE:   return "IDLE";
        case MOVING: return "MOVING";
        case LAG:    return "LAG";
        case BREAK:  return "BREAK";
        default:     return "UNKNOWN";
    }
}

// Макрос для смены состояния с логом
#define SET_STATE(newState, now) do { \
    if (arrowState != (newState)) { \
        Serial.printf("[%02d:%02d:%02d] ⚙️ FSM: %s → %s\n", \
            (now).hour(), (now).minute(), (now).second(), \
            stateName(arrowState), stateName(newState)); \
        arrowState = (newState); \
    } \
} while(0)

// Прототип отладочного дампа
void debugDump(DateTime now, bool microSwitchState);
