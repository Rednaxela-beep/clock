//config.h Конфигурация пинов и основных переменных 
#pragma once
#include <cstdint> // 
#include <Arduino.h> // даёт uint8_t, int32_t и т.д.
// Параметры движения стрелок
extern int StepsForMinute;      // шагов на минуту
extern float stepFraction;      // доля минутного хода (1.0 = полный, 0.1 = десятая часть)
extern int stepIntervalSec;     // интервал между стартами (считается из stepFraction)
extern int transitionTimeSec;   // время перехода (считается из baseTransitionSec * stepFraction)
extern const int baseTransitionSec; // базовое время для полного минутного хода
extern float stepperMaxSpeed;       // Макс. скорость шаговика (шагов/сек)
extern float stepperAcceleration;   // Ускорение (шагов/сек²)
// Сетевые настройки
#define WIFI_SSID     "5stars"
#define WIFI_PASSWORD "Vaio8010"
#define WEB_PORT      80

// Пин сервопривода боя
#define SERVO_PIN D6

// ====== Шаговый мотор стрелок ======
#define IN1 D3
#define IN2 D2
#define IN3 D1
#define IN4 D0

// Пин микрика
#define MICROSW_PIN D7

// Антидребезг (мс)
#define DEBOUNCE_DELAY 50

// Пины I2C для RTC
#define SDA_PIN 5
#define SCL_PIN 6
