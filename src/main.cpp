// main.cpp - Главный модуль. Отсчет времени и вызов в нужные моменты
#include "mqtt.h"  // заголовочный файл mqtt.h
#include <Arduino.h>
#include <time.h>  // для configTime, если понадобится позже
#include "wi-fi.h"
#include "main.h"
#include <ESP32Servo.h>
#include <Wire.h>  // для шаговика
#include <AccelStepper.h>
#include "ota.h"

// ====== Аппаратные объекты ======
Servo mg90;                                                         // Серво для молоточка
RTC_DS3231 rtc;                                                     // RTC модуль DS3231
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);  // HALF4WIRE — полушаговый режим 28BYJ-48

// ====== Глобальные переменные состояния ======
bool syncedThisHour = false;  // Флаг синхронизации один раз в час
bool systemReady = false;     // Флаг окончания загрузки
bool microSwRaw() {           // ====== Сырой статус концевика для веб-монитора ======
  return digitalRead(MICROSW_PIN);
}
// Глобальные объекты
volatile int pendingChimes = 0;
extern bool syncedThisHour;
void connectToWiFi();
uint32_t avgLoopUs = 0;

// -----------------------------------------------------------------------------
// Инициализация системы
void setup() {
  Serial.begin(115200);
  delay(500);  // Пауза для установки соединения с портом

  Serial.printf("🔖 Версия проекта: %s (собрано %s %s)\n",
                PROJECT_VERSION, BUILD_DATE, BUILD_TIME);

  chimesetup();     // Инициализация молоточка
  connectToWiFi();  // Подключение к WiFi

  Wire.begin(SDA_PIN, SCL_PIN);  // Шина RTC из config.cpp
  delay(50);                     // 🧘 Даем шине стабилизироваться

  // Инициализация скорости и ускорения мотора
  stepper.setMaxSpeed(stepperMaxSpeed);
  stepper.setAcceleration(stepperAcceleration);

  bool rtcReady = false;  // Проверка готовности RTC
  delay(150);             // 🧘 Дать шине стабилизироваться перед RTC
  for (int i = 0; i < 3; i++) {
    if (rtc.begin()) {
      rtcReady = true;
      Serial.println("👌 RTC модуль подключен");
      break;
    }
    delay(300);
  }
  // обновляем глобальный флаг
  rtcAvailable = rtcReady;
  if (!rtcAvailable) {
    Serial.println("⚠️ RTC модуль не найден, используем системное время");
  }

  delay(250);                          // Даем IDE время подключиться
  DateTime now = syncRTC();            // Синхронизация по NTP
  SET_STATE(IDLE, now);                // Начальное состояние FSM
  pinMode(MICROSW_PIN, INPUT_PULLUP);  // Подтяжка MICROSW_PIN к HIGH. При замыкании микрика на землю получаем чёткий LOW

  debugLogf("✅ Старт завершён. 🕰️ Текущее время: %02d:%02d:%02d %02d.%02d.%04d\n",
            now.hour(), now.minute(), now.second(),
            now.day(), now.month(), now.year());

  otaSetup();  // Инициализация OTA

  setupMQTT();  // Инициализация MQTT-клиента

  lastRtcMinute = now.minute();  // Чтобы FSM подождал реальной смены минуты
  systemReady = true;            // Система готова к работе
}
// -----------------------------------------------------------------------------
// Основной цикл
void loop() {
  extern uint32_t avgLoopUs;
  static uint32_t lastLoopUs = 0;

  uint32_t nowUs = micros();
  uint32_t loopTime = nowUs - lastLoopUs;
  lastLoopUs = nowUs;
  // Экспоненциальное сглаживание (10%)
  avgLoopUs = (avgLoopUs * 9 + loopTime) / 10;

  if (!systemReady) return;  // Защита от преждевременного вызова FSM
  DateTime now = getCurrentTime();
  int rtcMinute = now.minute();
  int currentSecond = now.second();

  int hour = now.hour() % 12;  // Приводим к 12-часовому формату
  if (hour == 0) hour = 12;

  // Вызов боя каждые полчаса (отложенный)
  if ((rtcMinute == 0 || rtcMinute == 30) && rtcMinute != lastStrikeMinute) {

    if (rtcMinute == 0) {
      pendingChimes = hour;  // флаг: «нужно ударить "hour" раз»
    } else {
      pendingChimes = 1;  // один удар в половину
    }

    lastStrikeMinute = rtcMinute;
  }
  // Двигаем шаговик, если нужно
  stepper.run();

  // if (stepper.distanceToGo() == 0) {  // Если шаговик доехал — переводим в IDLE и отключаем питание
  //   SET_STATE(IDLE, now);
  //   stepper.disableOutputs();
  // }

  handleHourlySync(now);                           // Синхронизация каждый час (определена в wi-fi.cpp)
  arrowFSM_update(now, rtcMinute, currentSecond);  // Обновление FSM стрелок

  otaLoop();          // Обработка OTA
  debugSerialLoop();  // единая точка входа для всех команд из Serial


  reconnectMQTT();  // Проверка подключения к MQTT
  mqttLoop();       // Поддержка соединения

  static unsigned long lastMqtt = 0;
  unsigned long nowMillis = millis();

  if (nowMillis - lastMqtt >= 1000) {
    lastMqtt = nowMillis;

    String rtcStr = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
    String uptimeStr = String(nowMillis / 1000) + "s";

    publishMetrics(now);  // Публикация метрик контроллера
  }
}
