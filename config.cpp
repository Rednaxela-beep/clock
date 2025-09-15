//config.cpp Определение главных переменных
#include "config.h"

int StepsForMinute = -6245; // Одна минута в полушаговом режиме = теоритически 6245 шагов
// Режим раз в минуту
float    stepFraction      = 1.0f;
uint8_t  stepIntervalSec   = 60;
uint8_t  transitionTimeSec = 20;