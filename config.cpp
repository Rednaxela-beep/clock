//config.cpp Определение главных переменных
#include "config.h"
// Флаги состояния времени
bool rtcAvailable = false;      // RTC модуль найден и отвечает
bool ntpLastSyncOk = false;     // Последняя синхронизация NTP успешна
String ntpLastSyncTime = "";    // Метка времени последней успешной синхронизации
String timeSource = "UNKNOWN";  // Источник времени: RTC / NTP / MILLIS
// Базовые метки для виртуального RTC
unsigned long baseMillis = 0;  // точка отсчёта
DateTime baseDateTime;         // реальное время на момент синхронизации
// MQTT параметры
const char* MQTT_SERVER = "broker.hivemq.com";    // публичный брокер HiveMQ
const int MQTT_PORT = 1883;                       // WebSocket TLS-порт
const char* MQTT_TOPIC = "ancient_clock/status";  // Топик

int StepsForMinute = -685;             // Одна минута в полушаговом режиме = теоритически 690 шагов
float stepperMaxSpeed = 550;           // (550) Скорость мотора - в теории до тысячи но на практике больше 750-800 не стоит
float stepperAcceleration = 250;       // (950) Поиск оптимального ускорения...
const int corrSteps = 0;               // Микрик срабатывает за столько шагов до нуля
const float correctionPercent = 0.5f;  // Небольшая поправка (0.1f=10%) положения стрелки в ключевом моменте корректировки
const int correctionOffset = round(StepsForMinute * correctionPercent);
bool microSwitchTriggered = false;
bool systemIdle = false;         // признак "система в покое"
unsigned long lastActiveMs = 0;  // время последней активности
// =========== Изменение Режима хода стрелки ===========
int transitionTimeSec = 3;  // Время перемещения стрелки - используется для предстарта
int stepIntervalSec = 60;   // Интервал шагов, сек

// ⚙️ Глобальные параметры боя (начальные значения)
int liftAngle = 10;           // Угол взвода (молоточек вверху)
int tailAngle = 25;           // Угол ухода привода из под молоточка (20 - ночной режим, 25-30 - стандартный бой)
int liftSpeed = 20;           // Скорость взвода (мс/шаг)
int pauseBetweenHits = 1000;  // Пауза между ударами (мс)
int lastStrikeMinute = -1;    // Глобальная переменная для предотвращения повтора боя часов
