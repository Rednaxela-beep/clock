// config.h Конфигурация пинов и основных переменных
#pragma once
#define PROJECT_NAME "Ancient Clock Digital Heart"
#define PROJECT_VERSION "1.3.1 Beta"  // Версия проекта
#define BUILD_DATE __DATE__      // Дата и Время сборки
#define BUILD_TIME __TIME__      // автоматически подставляется при компиляции

#include <cstdint>    //
#include <Arduino.h>  // даёт uint8_t, int32_t и т.д.
#include "secret.h"   // Подключаем приватные данные
#include <RTClib.h>   //  тип DateTime
// ====== Шаговый мотор стрелок (28BYJ-48 через ULN2003) ======
#define IN1 D10  // GPIO9 (физически D10)
#define IN2 D9   // GPIO8 (физически D9)
#define IN3 D8   // GPIO7  (физически D8)
#define IN4 D7   // GPIO44  (физически D7)
// ====== Серво боя ======
#define SERVO_PIN D3  // Пин управления сервоприводом
// ====== Концевик (микрик) ======
#define MICROSW_PIN D2  // Пин микрика
// ====== I2C для RTC DS3231M ======
#define SDA_PIN D4  // GPIO5  (физически D4)
#define SCL_PIN D1  // GPIO6  (физически D5)
// MQTT настройки
#define MQTT_SERVER "blog.umd.by"
#define MQTT_PORT   1883
#define MQTT_TOPIC_METRICS "ancient_clock/metrics"

// Флаги состояния, переменные и метки времени
const unsigned long DEBOUNCE_DELAY = 10;  // Антидребезг (мс)
extern bool rtcAvailable;
extern bool ntpLastSyncOk;
extern String ntpLastSyncTime;
extern String timeSource;
extern unsigned long baseMillis;  // точка отсчёта
extern DateTime baseDateTime;     // реальное время на момент синхронизации
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
// ---------------- NTP ----------------
extern const char* NTP_SERVERS[];
extern const int NTP_SERVER_COUNT;

extern const long GMT_OFFSET_SEC;
extern const int DAYLIGHT_OFFSET_SEC;

