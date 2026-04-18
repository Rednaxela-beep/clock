// arrow.h
#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "config.h"   // StepsForMinute, пины/настройки, константы

// Глобальное состояние стрелки
enum ArrowState {
  IDLE,
  MOVING,
  CORRECT_LAG,      // догоняющая коррекция (отставание)
  CORRECT_ADVANCE,  // коррекция опережения (возврат назад)
  CORRECT_FINE      // тонкая доводка после сработки микрика
};

extern int lastRtcMinute; // Видимость для Debug.cpp 
extern uint8_t invalidSecond;
extern bool applyCorrectionNextStep;
extern int correctionDeltaSteps;
extern bool stepperEnabled;

extern ArrowState arrowState;
void SET_STATE(ArrowState newState, DateTime now); // Прототип функции состояния
// FSM минутной стрелки
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchState);

// Концевик минутной стрелки (антидребезг + валидация)
bool microSw();
