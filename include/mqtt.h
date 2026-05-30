// mqtt.h
#pragma once
#include <RTClib.h>
#include <math.h>

void setupMQTT();
void reconnectMQTT();
void mqttLoop();                           // Обёртка
void publishMetrics(const DateTime& now);  // Функция публикации метрик