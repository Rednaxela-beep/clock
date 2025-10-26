// chimes.h Объвление нужных для боя переменных 
#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

// Сервообъект создаётся в main.cpp, но используется здесь
extern Servo sg90;

// Инициализация молоточка
void chimesetup();

// Плавный взвод молоточка
void smoothLift(int fromAngle, int toAngle);

// Запуск боя (count ударов)
void hit(int count);

// Обработка команд из Serial для управления боем
void chimesloop();

// Параметры боя (глобальные, чтобы можно было менять из других модулей)
extern int liftAngle;         // угол удержания молоточка
extern int tailAngle;         // угол удара
extern int liftSpeed;         // скорость взвода (мс/шаг)
extern int hitPrepDelay;      // пауза между взводом и ударом
extern int pauseBetweenHits;  // пауза между ударами
