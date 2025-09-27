//config.cpp Определение главных переменных
#include "config.h"

int StepsForMinute = -687; // Одна минута в полушаговом режиме = теоритически 690 шагов
float stepperMaxSpeed     = 200.0f;  // Скорость мотора - в теории до тысячи но на практике больше 800 не стоит
float stepperAcceleration = 750.0f;  // Поиск оптимального ускорения...
// Изменение Режима хода стрелки
int transitionTimeSec = 4;        // Время перемещения стрелки
int stepIntervalSec = 60;          // Интервал шагов, сек. Пересчитается в init
// Изменение напряжения
const int   VBAT_ADC_PIN        = 10;     // пример для ESP32
const float VBAT_DIVIDER        = 2.1f;   // делитель 2:1
const float ADC_REF             = 3.3f;   // опорное напряжение
// const int   batteryMeasureMinute = 15;    // измеряем на 15-й минуте часа