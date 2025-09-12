//config.h Конфигурация пинов и основных переменных 
#pragma once
extern int StepsForMinute;

// Параметры движения стрелок
// #define STEPPER_MAX_SPEED     650.0f   // шагов/сек
// #define STEPPER_ACCELERATION  350.0f   // шагов/сек²

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
