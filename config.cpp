//config.cpp Определение главных переменных
#include "config.h"

int StepsForMinute = 690;            // Одна минута в полушаговом режиме = теоритически 690 шагов
float stepperMaxSpeed = 200.0f;      // Скорость мотора - в теории до тысячи но на практике больше 800 не стоит
float stepperAcceleration = 500.0f;  // Поиск оптимального ускорения...
const int corrSteps = 500;           // Микрик срабатывает за столько шагов до нуля

// Изменение Режима хода стрелки (устарело)
int transitionTimeSec = 5;  // Время перемещения стрелки - используется для предстарта
int stepIntervalSec = 60;            // Интервал шагов, сек
// Изменение напряжения
const int VBAT_ADC_PIN = 10;      // пример для ESP32
const float VBAT_DIVIDER = 2.1f;  // делитель 2:1
const float ADC_REF = 3.3f;       // опорное напряжение
                                  // const int   batteryMeasureMinute = 15;    // измеряем на 15-й минуте часа