// mqtt.cpp — неблокирующий MQTT для Ancient Clock (v2)
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "main.h"
#include "arrow.h"
#include "mqtt.h"
#include "config.h"

extern uint32_t avgLoopUs;           // Среднее время цикла
extern long correctionSteps;         // Последняя коррекция в шагах
extern float lastCorrectionMinutes;  // Последняя коррекция в минутах

WiFiClient espClient;
PubSubClient client(espClient);

// Флаг: настройки уже отправлены после успешного подключения
static bool settingsPublished = false;

// Состояние MQTT-соединения
static bool mqttWasConnected = false;  // Было ли когда-то подключение
static bool mqttReportedDown = false;  // Уже сообщали о проблеме
static unsigned long lastMqttAttempt = 0;

// ================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==================

static void logSerial(const char* msg) {
  Serial.println(msg);
}

// Публикация события (лог) — каждое событие отдельным сообщением
void publishEvent(const char* msg) {
  if (!client.connected()) return;

  char payload[256];
  DateTime now = getCurrentTime();  // объявлена в main.h

  // В msg желательно не использовать кавычки, либо экранировать заранее
  snprintf(payload, sizeof(payload),
           "{"
           "\"ts\":\"%04d-%02d-%02d %02d:%02d:%02d\","
           "\"msg\":\"%s\""
           "}",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(),
           msg);

  client.publish(MQTT_TOPIC_EVENT, payload);
}

// Публикация настроек (редко меняющиеся / постоянные параметры)
// Вызывается из mqttLoop(), но реально отработает только один раз
// после успешного подключения.
void publishSettings() {
  if (!client.connected()) return;
  if (settingsPublished) return;

  char payload[256];

  // Предполагаем, что эти константы определены в config.h:
  // PROJECT_VERSION, BUILD_DATE, BUILD_TIME,
  // StepsForMinute, stepperMaxSpeed, stepperAcceleration
  snprintf(payload, sizeof(payload),
           "{"
           "\"fw\":\"%s\","
           "\"build_date\":\"%s\","
           "\"build_time\":\"%s\","
           "\"steps_per_minute\":%d,"
           "\"speed\":%d,"
           "\"accel\":%d"
           "}",
           PROJECT_VERSION,
           BUILD_DATE,
           BUILD_TIME,
           StepsForMinute,
           stepperMaxSpeed,
           stepperAcceleration);

  client.publish(MQTT_TOPIC_SETTINGS, payload);
  settingsPublished = true;
}

// ================== ОСНОВНОЙ ФУНКЦИОНАЛ MQTT ==================

// Инициализация MQTT-клиента
void setupMQTT() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

// Неблокирующее переподключение к MQTT
void reconnectMQTT() {
  // Если Wi‑Fi не подключен — даже не пытаемся
  if (WiFi.status() != WL_CONNECTED) {
    if (client.connected()) {
      client.disconnect();
    }
    mqttWasConnected = false;
    // Не спамим логом, просто молча ждём Wi‑Fi
    return;
  }

  // Если уже подключены — просто фиксируем состояние и выходим
  if (client.connected()) {
    if (!mqttWasConnected) {
      mqttWasConnected = true;
      mqttReportedDown = false;
      logSerial("🟢 MQTT: соединение установлено");
      publishEvent("MQTT connected");
    }
    return;
  }

  // Здесь: Wi‑Fi есть, MQTT нет
  unsigned long now = millis();

  // Пытаемся подключаться не чаще, чем раз в 2 минуты
  if (now - lastMqttAttempt < 120000UL) {
    return;
  }
  lastMqttAttempt = now;

  logSerial("🔄 MQTT: попытка подключения к брокеру...");
  bool ok = client.connect("AncientClockClient", MQTT_USER, MQTT_PASS);

  if (ok) {
    mqttWasConnected = true;
    mqttReportedDown = false;
    settingsPublished = false;  // отправим настройки заново
    logSerial("🟢 MQTT: подключение успешно");
    publishEvent("MQTT connected");

    // Здесь в будущем можно добавить подписки на команды:
    // client.subscribe(MQTT_TOPIC_COMMANDS);
  } else {
    if (!mqttReportedDown) {
      mqttReportedDown = true;
      logSerial("⚠️ MQTT: брокер недоступен, переходим в деградированный режим");
      // publishEvent здесь не имеет смысла — MQTT всё равно не работает
    }
  }
}

// Публикация телеметрии (пульс системы)
void publishMetrics(const DateTime& now) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!client.connected()) return;

  char payload[384];

  uint32_t ramFree = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
  uint32_t ramMin = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
  uint32_t ramTotal = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);

  float ramFreePercent = (ramFree * 100.0f) / ramTotal;
  float ramMinPercent = (ramMin * 100.0f) / ramTotal;

  float loopMs = roundf((avgLoopUs / 1000.0f) * 100) / 100;

  char corrBuf[16];
  if (isnan(lastCorrectionMinutes)) {
    strcpy(corrBuf, "null");
  } else {
    snprintf(corrBuf, sizeof(corrBuf), "%.2f", lastCorrectionMinutes);
  }

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
           "\"ram_min_percent\":%.1f,"
           "\"fsm\":\"%s\","
           "\"corr_min\":%s"
           "}",
           now.hour(), now.minute(), now.second(),
           millis() / 1000,
           loopMs,
           WiFi.RSSI(),
           correctionSteps,
           ramFree,
           ramMin,
           ramFreePercent,
           ramMinPercent,
           fsmToText(arrowState, (now.minute() + 1) % 60, pendingChimes),
           corrBuf);

  client.publish(MQTT_TOPIC_METRICS, payload);
}

// Обёртка для client.loop(), чтобы вызывать из main.cpp
void mqttLoop() {
  client.loop();

  // Если подключены — гарантируем, что настройки один раз улетят
  if (client.connected()) {
    publishSettings();
  }
}
// КОНЕЦ mqtt.cpp