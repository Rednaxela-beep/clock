//config.cpp Определение главных переменных
#include "config.h"

int StepsForMinute = -6247; // Одна минута в полушаговом режиме = теоритически 6245 шагов
float stepperMaxSpeed     = 750.0f;  // Скорость мотора - в теории до тысячи но на практике больше 750-800 не стоит
float stepperAcceleration = 500.0f;  // Поиск оптимального ускорения...
// Изменение Режима хода стрелки
float stepFraction = 1.0f;         // множитель (f указывает, что это float literal)
int stepIntervalSec = 60;          // пересчитается в init
int transitionTimeSec = 14;        // пересчитается в init
// const int baseTransitionSec = 14;  // базовое время для полного минутного хода - высчитывается в arrowInitParams в arrow.cpp