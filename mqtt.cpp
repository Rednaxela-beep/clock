// mqtt.cpp — публикация параметров на MQTT сервер
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "mqtt.h"
#include "config.h"

// Глобальные объекты клиента
WiFiClient espClient;
PubSubClient client(espClient);

// Инициализация MQTT-клиента
void setupMQTT() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

// Проверка подключения и переподключение при необходимости
void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("AncientClockClient")) {
      // Успешное подключение
    } else {
      delay(5000); // Пауза перед повторной попыткой
    }
  }
}

// Публикация параметров RTC и uptime
void publishClockStatus(const String& rtc, const String& uptime) {
  String payload = "{\"rtc\":\"" + rtc + "\",\"uptime\":\"" + uptime + "\"}";
  client.publish(MQTT_TOPIC, payload.c_str());
}

// Обёртка для client.loop(), чтобы вызывать из main.cpp
void mqttLoop() {
  client.loop();
}
