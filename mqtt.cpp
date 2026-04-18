// mqtt.cpp — публикация параметров на MQTT сервер
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "mqtt.h"
#include "config.h"

extern uint32_t avgLoopUs;  // Среднее время цикла
extern long correctionSteps;

WiFiClient espClient;  // Глобальные объекты клиента
PubSubClient client(espClient);

void setupMQTT() {  // Инициализация MQTT-клиента
  client.setServer(MQTT_SERVER, MQTT_PORT);
}
// Функция публикации метрик
void publishMetrics(const DateTime& now) {
  char payload[256];

  uint32_t ramFree  = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  uint32_t ramMin   = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
  uint32_t ramTotal = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);  // только для расчёта %

  float ramFreePercent = (ramFree * 100.0f) / ramTotal;
  float ramMinPercent  = (ramMin  * 100.0f) / ramTotal;

  float loopMs = roundf((avgLoopUs / 1000.0f) * 100) / 100;

  snprintf(payload, sizeof(payload),
           "{"
             "\"rtc\":\"%02d:%02d:%02d\","
             "\"uptime\":%lu,"
             "\"loop_ms\":%.2f,"
             "\"rssi\":%d,"
             "\"correctionSteps\":%ld,"
             "\"ram_free\":%u,"
             "\"ram_min_free\":%u,"
             "\"ram_free_percent\":%.1f,"
             "\"ram_min_percent\":%.1f"
           "}",
           now.hour(), now.minute(), now.second(),
           millis() / 1000,
           loopMs,
           WiFi.RSSI(),
           correctionSteps,
           ramFree,
           ramMin,
           ramFreePercent,
           ramMinPercent
  );

  client.publish(MQTT_TOPIC_METRICS, payload);
}

// Проверка подключения и переподключение при необходимости
void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("AncientClockClient", MQTT_USER, MQTT_PASS)) {
      // Успешное подключение
    } else {
      delay(5000);
    }
  }
}
// Обёртка для client.loop(), чтобы вызывать из main.cpp
void mqttLoop() {
  client.loop();
}
// ========== КОНЕЦ mqtt.cpp ==========