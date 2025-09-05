#include "driver/uart.h"
#pragma once
void arrowFSM_update(int arrowMinute, int currentSecond, bool microSwitchHit);

enum ArrowState { // Конечный автомат управления стрелкой
  UNKNOWN,  // Первое состояние
  IDLE,     // Ожидание
  MOVING,   // Двигаем стрелку
  LAG,      // Корректировка отставания
  BREAK     // Корректировка спешащей стрелки
};

ArrowState arrowState = IDLE;