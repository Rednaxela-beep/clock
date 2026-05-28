//config.cpp Определение главных переменных
#include "config.h"
#include <limits.h>   // для LONG_MIN
#include <math.h>     // для NAN и isnan()
// Флаги состояния времени
bool rtcAvailable = false;      // RTC модуль найден и отвечает
bool ntpLastSyncOk = false;     // Последняя синхронизация NTP успешна
String ntpLastSyncTime = "";    // Метка времени последней успешной синхронизации
String timeSource = "UNKNOWN";  // Источник времени: RTC / NTP / MILLIS
// Базовые метки для виртуального RTC
unsigned long baseMillis = 0;  // точка отсчёта
DateTime baseDateTime;         // реальное время на момент синхронизации

int StepsForMinute = -685;             // Одна минута в полушаговом режиме = теоритически 690 шагов
float stepperMaxSpeed = 550;           // (550) Скорость мотора - в теории до тысячи но на практике больше 750-800 не стоит
float stepperAcceleration = 250;       // (950) Поиск оптимального ускорения...
const int corrSteps = 0;               // Микрик срабатывает за столько шагов до нуля
const float correctionPercent = 0.4f;  // Небольшая поправка (0.4f=40%) положения стрелки в ключевом моменте корректировки
// const int correctionOffset = round(StepsForMinute * correctionPercent);
bool systemIdle = false;         // признак "система в покое"
unsigned long lastActiveMs = 0;  // время последней активности

// =========== Фиксация позиции срабатывания микрика ===========
long moveStartPosition = 0;      // позиция шаговика в начале минутного хода
long stepAtTriggerPos = LONG_MIN; // позиция срабатывания микрика внутри минутного хода (0..|StepsForMinute|-1), LONG_MIN — не зафиксировано
// Для отставания (догон)
long lagStartPosition = 0;       // позиция шаговика в начале догонки
long lagStepsToTrigger = 0;      // сколько шагов прошло в догонке до срабатывания микрика
// Диагностические значения для публикации
long errorSteps = 0;             // истинная ошибка в шагах (signed)
float errorMinutes = NAN;        // истинная ошибка в минутах
// =========== Изменение Режима хода стрелки ===========
int transitionTimeSec = 3;  // Время перемещения стрелки - используется для предстарта
int stepIntervalSec = 60;   // Интервал шагов, сек

// ⚙️ Глобальные параметры боя (начальные значения)
int liftAngle = 10;           // Угол взвода (молоточек вверху)
int tailAngle = 25;           // Угол ухода привода из под молоточка (20 - ночной режим, 25-30 - стандартный бой)
int liftSpeed = 20;           // Скорость взвода (мс/шаг)
int pauseBetweenHits = 1000;  // Пауза между ударами (мс)
int lastStrikeMinute = -1;    // Глобальная переменная для предотвращения повтора боя часов
// ⚙️ Глобальные параметры синхронизации NTP
const char* NTP_SERVERS[] = {
    "192.168.137.199",
    "time.belgim.by",
    "1.by.pool.ntp.org",
    "2.by.pool.ntp.org",
    "time.windows.com"
};
const int NTP_SERVER_COUNT = sizeof(NTP_SERVERS) / sizeof(NTP_SERVERS[0]);
const long GMT_OFFSET_SEC = 3 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;
