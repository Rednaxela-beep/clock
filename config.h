// config.h Конфигурация пинов и основных переменных
#pragma once
#include <cstdint>    //
#include <Arduino.h>  // даёт uint8_t, int32_t и т.д.
#include "secret.h"   // Подключаем приватные данные
#define PROJECT_NAME "Ancient Clock Digital Heart"
#define PROJECT_VERSION "1.1.1"  // Версия проекта
#define BUILD_DATE __DATE__      // Дата и время сборки
#define BUILD_TIME __TIME__      // автоматически подставляется при компиляции

// Параметры движения стрелок
extern int StepsForMinute;             // шагов на минуту
extern const int corrSteps;            // Ожидаемая до нуля позиция срабатывания микрика
extern const int baseTransitionSec;    // базовое время для полного минутного хода
extern float stepperMaxSpeed;          // Макс. скорость шаговика (шагов/сек)
extern float stepperAcceleration;      // Ускорение (шагов/сек²)
extern int stepIntervalSec;            // интервал между стартами
extern int transitionTimeSec;          // время перехода
extern const float correctionPercent;  // Небольшая корректировка положения по микрику
extern const int correctionOffset;     // То же самое, только в абс.значении
extern int lastStrikeMinute;           // Глобальная переменная для  предотвращения повтора боя часов
extern bool microSwitchTriggered;

// Сетевые настройки
#define WEB_PORT 80

// Пин сервопривода боя
#define SERVO_PIN D8

// ====== Шаговый мотор стрелок ======
#define IN1 D3
#define IN2 D2
#define IN3 D1
#define IN4 D0

// Пин микрика
#define MICROSW_PIN D7

// Антидребезг (мс)
const unsigned long DEBOUNCE_DELAY = 10;

// Пины I2C для RTC
#define SDA_PIN 5
#define SCL_PIN 6