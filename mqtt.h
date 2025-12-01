#pragma once
#include <Arduino.h>   // чтобы был доступен тип String

void setupMQTT();
void reconnectMQTT();
void publishClockStatus(const String& rtc, const String& uptime);
void mqttLoop();   // Обёртка