// main.cpp Главный модуль. Отсчет времени и вызов в нужные моменты
// arrow перевод стрелок
// wi-fi подключение по WiFi и синхронизации времени (RTC)
// chimes Бой молоточком
#include <Arduino.h>
#include <time.h>  // для configTime, если понадобится позже

#include "wi-fi.h"
#include "main.h"
#include <ESP32Servo.h>
#include <Wire.h>  // вроде это для шаговика
#include <AccelStepper.h>

// ====== Аппаратные объекты ======
Servo sg90;      // Серво для молоточка
RTC_DS3231 rtc;  // RTC модуль DS3231

AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);  // HALF4WIRE — полушаговый режим 28BYJ-48

// ====== Глобальные переменные состояния ======
bool syncedThisHour = false;  // Флаг синхронизации один раз в час
bool systemReady = false;     // Флаг окончания загрузки
// Глобальные объекты
extern Servo sg90;
extern RTC_DS3231 rtc;
extern AccelStepper stepper;
extern bool syncedThisHour;

void connectToWiFi();

// -----------------------------------------------------------------------------
// Инициализация системы
// -----------------------------------------------------------------------------
void setupMain() {
  Serial.begin(19200);
  delay(500);  // Пауза для установки соединения с портом

  Serial.printf("🔖 Версия проекта: %s (собрано %s %s)\n",
                PROJECT_VERSION, BUILD_DATE, BUILD_TIME);

  chimesetup();     // Инициализация молоточка
  connectToWiFi();  // Подключение к WiFi

  Wire.begin(5, 6);  // Шина RTC: SDA=D4 (GPIO5), SCL=D5 (GPIO6)
  delay(50);         // 🧘 Даем шине стабилизироваться

  // Инициализация скорости и ускорения мотора
  stepper.setMaxSpeed(stepperMaxSpeed);
  stepper.setAcceleration(stepperAcceleration);

  bool rtcReady = false;  // Проверка готовности RTC
  delay(150);             // 🧘 Дать шине стабилизироваться перед RTC
  for (int i = 0; i < 3; i++) {
    if (rtc.begin()) {
      rtcReady = true;
      break;
    }
    delay(300);
  }
  // if (!rtcReady) {
  //   Serial.println("❌ RTC не найден! Перезагружаем контроллер.");
  //   delay(1000);    // Дать время на вывод
  //   ESP.restart();  // 🔁 Мягкая перезагрузка
  // }

  delay(250);                // Даем IDE время подключиться
  DateTime now = syncRTC();  // Читаем актуальное время
  SET_STATE(IDLE, now);      // Начальное состояние FSM
  webMonitorBegin();         // Инициализация Веб Монитора

  debugLogf("✅ Старт завершён. 🕰️ Текущее время RTC: %02d:%02d:%02d %02d.%02d.%04d\n",
            now.hour(), now.minute(), now.second(),
            now.day(), now.month(), now.year());

  lastRtcMinute = now.minute();           // Чтобы FSM подождал реальной смены минуты
  float vbat = measureBattery();          // При старте сразу измеряем наряжение
  lastBatteryVoltage = measureBattery();  // Обновляем переменную которая хранит измеренное напряжение
  batteryVoltage(lastBatteryVoltage);
  // batteryVoltage(vbat);  // и сразу выводим в debug и веб
  systemReady = true;  // Система готова к работе
}

// -----------------------------------------------------------------------------
// Основной цикл
// -----------------------------------------------------------------------------
void loopMain() {
  if (!systemReady) return;  // Защита от преждевременного вызова FSM

  DateTime now = rtc.now();
  int rtcMinute = now.minute();
  int currentSecond = now.second();
  bool microSwitchState = microSw();

  int hour = now.hour() % 12;  // Приводим к 12-часовому формату
  if (hour == 0) hour = 12;

  // Двигаем шаговик, если нужно
  stepper.run();

  if (stepper.distanceToGo() == 0) {  // Если шаговик доехал — переводим в IDLE и отключаем питание
    SET_STATE(IDLE, now);
    stepper.disableOutputs();
  }

  handleHourlySync(now);  // Синхронизация каждый час (определена в wi-fi.cpp)

  arrowFSM_update(now, rtcMinute, currentSecond, microSwitchState);  // Обновление FSM стрелок

  webMonitorLoop();  // Обновление Веб Монитора

  // Отладочный вывод по команде с Serial
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'd') {
      DateTime now = rtc.now();
      debugDump(now, microSwitchState);
    }
  }
}
