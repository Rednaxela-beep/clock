// wi-fi.cpp функции сети TCP/IP
#include <Arduino.h>  // обязательно первым
#include <WiFi.h>
#include <time.h>     // для NTP
#include "main.h"     // чтобы видеть rtc, syncedThisHour, arrowState, getCurrentTime и прочее
#include "wi-fi.h"
#include "config.h"   // WIFI_SSID, WIFI_PASSWORD, NTP_SERVERS и т.д.

// -----------------------------------------------------------------------------
// Подключение к Wi-Fi (устранение 'WiFi' was not declared in this scope и 'WL_CONNECTED' was not declared in this scope)
// -----------------------------------------------------------------------------
void connectToWiFi() {
  Serial.print("🔌 Подключаемся к Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);          // режим клиента
  WiFi.setAutoReconnect(true);  // автоматическое переподключение
  WiFi.persistent(true);        // сохранять настройки в NVS

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {  // 40 × 500мс = 20 сек
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi подключен!");
    Serial.print("📡 IP адрес: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Не удалось подключиться к Wi-Fi!");
  }
}
// -----------------------------------------------------------------------------
// Обёртка для получения текущего времени
// -----------------------------------------------------------------------------
DateTime getCurrentTime() {
  if (rtcAvailable) {
    timeSource = "RTC";  // Если RTC отвечает — используем его
    return rtc.now();
  }

  // Если RTC недоступен — используем системное время ESP32
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    timeSource = "NTP";
    return DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec);
  }

  // Фолбэк на millis()
  unsigned long elapsed = millis() - baseMillis;
  timeSource = "MILLIS";
  return baseDateTime + TimeSpan(elapsed / 1000);
}

// -----------------------------------------------------------------------------
// Синхронизация RTC по NTP
// -----------------------------------------------------------------------------
DateTime syncRTC() {
  Serial.println();
  Serial.println("----- Синхронизация RTC по NTP -----");

  struct tm timeinfo;

  // 0) Проверяем Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Wi-Fi не подключён — пропускаем NTP");
    timeSource = rtcAvailable ? "RTC" : "MILLIS";
    return getCurrentTime();
  }

  // 1) Сбрасываем системное время ESP32, чтобы избежать ложных NTP-ответов
  struct timeval tv = {0};
  settimeofday(&tv, nullptr);

  // 2) Перебираем NTP-серверы
  for (int i = 0; i < NTP_SERVER_COUNT; i++) {
    Serial.print("🌐 NTP попытка: ");
    Serial.println(NTP_SERVERS[i]);

    // Запуск NTP через configTime
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVERS[i]);

    // 3) Ждём реального ответа NTP (до 10 секунд)
    Serial.print("⏳ Ждём NTP");
    int tries = 0;
    while (!getLocalTime(&timeinfo) && tries < 50) {
      Serial.print(".");
      delay(200);
      tries++;
    }

    if (!getLocalTime(&timeinfo)) {
      Serial.println(" ❌ NTP не ответил");
      continue;  // пробуем следующий сервер
    }

    Serial.println(" ✅");

    // 4) Преобразуем в DateTime
    DateTime ntpTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    );

    // 5) Если RTC есть — обновляем его
    if (rtcAvailable) {
      DateTime rtcTime = rtc.now();
      long diff = ntpTime.unixtime() - rtcTime.unixtime();

      Serial.print(ntpTime.timestamp());
      Serial.print(";📊 Разница RTC vs NTP: ");
      Serial.print(diff);
      Serial.println(" сек");

      if (abs(diff) > 1) {
        rtc.adjust(ntpTime);
        Serial.print(ntpTime.timestamp());
        Serial.println(";✅ RTC синхронизировано");
      } else {
        Serial.println("⏱ RTC уже точное");
      }
    }

    // 6) Обновляем виртуальные часы
    baseMillis = millis();
    baseDateTime = ntpTime;
    timeSource = "NTP";

    Serial.print(ntpTime.timestamp());
    Serial.println(";✅ Старт завершён. 🕰️ Текущее время: "
                   + ntpTime.timestamp(DateTime::TIMESTAMP_DATE) + " "
                   + ntpTime.timestamp(DateTime::TIMESTAMP_TIME));

    return ntpTime;
  }

  // 7) Если все NTP-серверы недоступны
  Serial.println("❌ Все NTP-серверы недоступны — fallback");

  timeSource = rtcAvailable ? "RTC" : "MILLIS";
  return getCurrentTime();
}
// -----------------------------------------------------------------------------
// Ежечасная синхронизация (на 45‑й минуте, если FSM в IDLE)
// -----------------------------------------------------------------------------
void handleHourlySync(DateTime now) {
  static int lastHour = -1;

  if (now.hour() != lastHour) {
    lastHour = now.hour();
    syncedThisHour = false;
  }

  if (now.minute() == 45 && !syncedThisHour && arrowState == IDLE) {
    DateTime syncedTime = syncRTC();

    if (syncedTime.isValid()) {  // --- Синхронизация RTC ---
      syncedThisHour = true;
      String logMessage = "✅ RTC синхронизировано: " + syncedTime.timestamp();
      debugLogf(logMessage.c_str());
    } else {
      Serial.println("⚠️ Синхронизация не удалась");
    }
  }
}
// Конец wi-fi.cpp