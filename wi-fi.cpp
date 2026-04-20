// wi-fi.cpp функции сети TCP/IP
#include <Arduino.h>  // обязательно первым
#include <WiFi.h>
#include <time.h>     // для NTP
#include <WiFiUdp.h>  // Используем UDP для NTP
#include "main.h"     // чтобы видеть rtc, syncedThisHour, arrowState, getCurrentTime и прочее
#include "wi-fi.h"
#include "config.h"  // WIFI_SSID, WIFI_PASSWORD, NTP_SERVERS и т.д.

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
// Синхронизация RTC по NTP (собственный клиент)
// -----------------------------------------------------------------------------
bool getNtpTime(const char* server, DateTime& outTime);  // ПРОТОТИП getNtpTime ДЛЯ КОМПИЛЯТОРА
DateTime syncRTC() {
  Serial.println();
  Serial.println("----- Синхронизация RTC по NTP (UDP) -----");

  // 0) Проверяем Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Wi-Fi не подключён — пропускаем NTP");
    timeSource = rtcAvailable ? "RTC" : "MILLIS";
    return getCurrentTime();
  }

  DateTime ntpTime;
  bool success = false;

  // 1) Перебираем NTP-серверы
  for (int i = 0; i < NTP_SERVER_COUNT; i++) {
    Serial.print("🌐 NTP попытка: ");
    Serial.println(NTP_SERVERS[i]);

    if (getNtpTime(NTP_SERVERS[i], ntpTime)) {
      Serial.println("✅ NTP ответил");
      success = true;
      break;
    } else {
      Serial.println("❌ NTP не ответил");
    }
  }

  if (!success) {
    Serial.println("❌ Все NTP-серверы недоступны — fallback");
    timeSource = rtcAvailable ? "RTC" : "MILLIS";
    return getCurrentTime();
  }

  // 2) Если RTC есть — сверяем и при необходимости обновляем
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

  // 3) Обновляем виртуальные часы
  baseMillis = millis();
  baseDateTime = ntpTime;
  timeSource = "NTP";

  Serial.print(ntpTime.timestamp());
  Serial.println(";✅ Старт завершён. 🕰️ Текущее время: "
                 + ntpTime.timestamp(DateTime::TIMESTAMP_DATE) + " "
                 + ntpTime.timestamp(DateTime::TIMESTAMP_TIME));
  return ntpTime;
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

// ---------- Минимальный UDP‑NTP клиент ----------
WiFiUDP ntpUDP;
bool getNtpTime(const char* server, DateTime& outTime) {
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[NTP_PACKET_SIZE];

  // 1) Очищаем буфер
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // 2) Формируем NTP-запрос
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode

  // 3) Резолвим сервер
  IPAddress ntpIP;
  if (!WiFi.hostByName(server, ntpIP)) {
    Serial.println("❌ DNS не смог резолвить сервер");
    return false;
  }

  // 4) Отправляем пакет
  ntpUDP.begin(2390);  // локальный порт
  ntpUDP.beginPacket(ntpIP, 123);
  ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();

  // 5) Ждём ответ (до 1000 мс)
  uint32_t start = millis();
  while (millis() - start < 1000) {
    int size = ntpUDP.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);

      // 6) Извлекаем время (секунды с 1900 года)
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = (highWord << 16) | lowWord;

      // 7) Переводим в Unix-время
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      outTime = DateTime(epoch + GMT_OFFSET_SEC + DAYLIGHT_OFFSET_SEC); // Получили время и добавили смещения часового пояса и слетнее время
      return true;
    }
    delay(10);
  }
  return false;  // таймаут
}
// Конец wi-fi.cpp