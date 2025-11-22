// ota.cpp Модуль обновления On-The-Air
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "debug.h"
#include "ota.h"

void otaSetup() {
  ArduinoOTA.setHostname("clock");
  // ArduinoOTA.setPassword("your_ota_pass");

  ArduinoOTA.onStart([]() {
    Serial.println("🔄 OTA: начало обновления...");
    debugLogf("🔄 OTA: обновление началось...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("✅ OTA: обновление завершено!");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("❌ OTA ошибка [%u]\n", error);
  });

  ArduinoOTA.begin();
  Serial.println("📡 OTA готова к прошивке по Wi-Fi");
}

void otaLoop() {
  ArduinoOTA.handle();
}
