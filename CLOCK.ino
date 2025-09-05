// ===========Главный модуль. Отсчет времени и вызов в нужные моменты ===========
// arrow.ino перевод стрелок
// wifi.ino подключение по WiFi и синхронизации времени (RTC)
// chimes.ino Бой молоточком
#include <WiFi.h>
#include <ESP32Servo.h>
Servo sg90;
#include <Wire.h>
#include <RTClib.h>  // RealTimeClock module
RTC_DS3231 rtc;

// =========== Шаговый мотор стрелок ===========
#include <AccelStepper.h>  // Шаговый мотор
#define IN1 D3
#define IN2 D2
#define IN3 D1
#define IN4 D0
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);  // HALF4WIRE - полушаговый режим 28BYJ-48

// =========== Обработка микрика и стрелок ===========
#include "arrow.h"
const int microSw_PIN = D7;    // Второй контакт микрика
const int debounceDelay = 50;  // Антидребезг, миллисекунд
bool lastStableState = LOW;    // В свбодном состоянии микрик зпмкнут на GND - LOW
int lastReading = LOW;
unsigned long lastDebounceTime = 0;
bool microSwTriggered = false;  // глобальный флаг срабатывания микрика, сбрасывается после обработки

//
int arrowMinute = -1;         // Положение стрелки (в минутах)
int lastRtcMinute = -1;       // Сохраняем предыдущую минуту для сравнения
bool syncedThisHour = false;  // Чтобы синхронизировать только один раз в нужную минуту

bool systemReady = false;  // глобальный флаг окончания загрузки

void setup() {
  Serial.begin(19200);
    delay(500);             // Пауза для установки соединения
  chimesetup();           // Вызываем инициализацию молоточка
  connectToWiFi();        // Подключаемся к WiFi
  Wire.begin(5, 6);       // Шина модуля RTC - SDA=D4 (GPIO5), SCL=D5 (GPIO6)
    delay(50);     // 🧘 Даем шине стабилизироваться
  bool rtcReady = false;  // Для RTC модуля
  // Параметры движения стрелки
  stepper.setMaxSpeed(900.0);          // Макс.скорость шаговика в шагах/сек в полушаговом режиме
  stepper.setAcceleration(350.0);      // Ускорение в шагах/сек²
  pinMode(microSw_PIN, INPUT_PULLUP);  // Сигнал от НЗ микрика. LOW → HIGH каждые полчаса

  delay(100);       // 🧘 Дать шине стабилизироваться перед RTC
  for (int i = 0; i < 3; i++) {
    if (rtc.begin()) {
      rtcReady = true;
      break;
    }
    delay(300);
  }
  if (!rtcReady) {
    Serial.println("❌ RTC не найден! Перезагружаем контроллер.");
//    while (1)
    delay(1000);    // Дать время на вывод
    ESP.restart();  // 🔁 Мягкая перезагрузка контроллера
  }

  delay(250);                  // даём IDE время подключиться
  DateTime now = syncRTC();    // читаем актуальное значение
  arrowMinute = now.minute();  // 🧭 Полагаем, что стрелка уже выставлена на текущее время
  arrowState = IDLE;           // и пожтому не нужно никуда ехать
  Serial.printf("✅ Старт завершён. 🕰️ Текущее время RTC: %02d:%02d:%02d %02d.%02d.%04d\n",
                now.hour(), now.minute(), now.second(),
                now.day(), now.month(), now.year());
  lastRtcMinute = now.minute();
  systemReady = true;  // Устанавливаем флаг  окончания загрузки
}

void loop() {
  if (!systemReady) return;  // Защита от преждевременного вызова FSM
  DateTime now = rtc.now();
  int rtcMinute = now.minute();  // Текущее время RTC
  int currentSecond = now.second();
  bool microSwitchState = digitalRead(microSw_PIN) == LOW;

  int hour = now.hour() % 12;  // Вычисляем час
  hour = (hour == 0) ? 12 : hour;

//  if (arrowState == MOVING) {  // Обслуживание движения стрелки
    stepper.run();             // Двигаем шаговик, если нужно

    if (stepper.distanceToGo() == 0) {
      arrowState = IDLE;  //
      stepper.disableOutputs();
    }
//  }
microSw();  // Обработка срабатываний концевика стрелок
handleHourlySync(now); // Синхронизация каждый час. Определена в wifi.ino
arrowFSM_update(now, arrowMinute, rtcMinute, currentSecond, microSwitchState);
}
