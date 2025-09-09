// arrow.h
#include "driver/uart.h"
#pragma once
void arrowFSM_update(int arrowMinute, int currentSecond, bool microSwitchHit);

// Объявление глобального состояния (определяется в arrow.ino)
enum ArrowState {
  IDLE,     // Ожидание
  MOVING,   // Двигаем стрелку
  LAG,      // Корректировка отставания
  BREAK     // Корректировка спешащей стрелки
};

extern ArrowState arrowState; // только объявление, без инициализации