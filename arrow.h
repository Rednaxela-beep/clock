#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include "config.h"   // StepsForMinute, пины/настройки, константы

// Глобальное состояние стрелки
enum ArrowState {
    IDLE,     // Ожидание
    MOVING,   // Двигаем стрелку
};

extern ArrowState arrowState;
extern DateTime arrowStateChangedAt;
void SET_STATE(ArrowState newState, DateTime now); // Прототип функции состояния
// FSM минутной стрелки
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchState);

// Концевик минутной стрелки (антидребезг + валидация)
bool microSw();
