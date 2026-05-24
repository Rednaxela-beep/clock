// mqtt.h
#pragma once
#include <Arduino.h>  // чтобы был доступен тип String
#include <RTClib.h>
#include <math.h>

void setupMQTT();
void reconnectMQTT();
void mqttLoop();                           // Обёртка
void publishMetrics(const DateTime& now);  // Функция публикации метрик