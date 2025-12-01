//config.cpp Определение главных переменных
#include "config.h"
// MQTT параметры
const char* MQTT_SERVER = "test.mosquitto.org";   // публичный брокер
const int MQTT_PORT = 1883;                       // стандартный порт
const char* MQTT_TOPIC = "ancient_clock/status";  // Топик

int StepsForMinute = 688;              // Одна минута в полушаговом режиме = теоритически 690 шагов
float stepperMaxSpeed = 800.0f;        // Скорость мотора - в теории до тысячи но на практике больше 750-800 не стоит
float stepperAcceleration = 500.0f;    // Поиск оптимального ускорения...
const int corrSteps = 500;             // Микрик срабатывает за столько шагов до нуля
const float correctionPercent = 0.1f;  // Небольшая поправка (0.1f=10%) положения стрелки в ключевом моменте корректировки
const int correctionOffset = round(StepsForMinute * correctionPercent);
bool microSwitchTriggered = false;
bool systemIdle = false;       // признак "система в покое"
unsigned long lastActiveMs = 0; // время последней активности
// =========== Изменение Режима хода стрелки =========== 
int transitionTimeSec = 3;  // Время перемещения стрелки - используется для предстарта
int stepIntervalSec = 60;   // Интервал шагов, сек

// ⚙️ Глобальные параметры боя (начальные значения)
int liftAngle = 0;            // Угол взвода (молоточек вверху)
int tailAngle = 25;           // Угол ухода привода из под молоточка (20 - ночной режим, 25-30 - стандартный бой)
int liftSpeed = 20;           // Скорость взвода (мс/шаг)
int pauseBetweenHits = 1000;  // Пауза между ударами (мс)
int lastStrikeMinute = -1;    // Глобальная переменная для предотвращения повтора боя часов
