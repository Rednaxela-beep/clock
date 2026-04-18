// wi-fi.cpp функции сети TCP/IP
#include <Arduino.h>  // обязательно первым
#include <WiFi.h>
#include <time.h>  // для NTP

#include "main.h"  // чтобы видеть rtc, syncedThisHour, arrowState, getCurrentTime и прочее
#include "wi-fi.h"

// -----------------------------------------------------------------------------
// Подключение к Wi-Fi (устранение 'WiFi' was not declared in this scope и 'WL_CONNECTED' was not declared in this scope)
// -----------------------------------------------------------------------------
#include "config.h"  // WIFI_SSID, WIFI_PASSWORD

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
// ====== Обёртка для получения времени ======
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
  const char* ntpServers[] = {
    "time.belgim.by",
    "1.by.pool.ntp.org",
    "2.by.pool.ntp.org",
    "time.windows.com"
  };
  const int ntpServerCount = sizeof(ntpServers) / sizeof(ntpServers[0]);
  const long gmtOffset_sec = 3 * 3600;
  const int daylightOffset_s = 0;

  struct tm timeinfo;
  for (int i = 0; i < ntpServerCount; i++) {
    Serial.print("🌐 NTP попытка: ");
    Serial.println(ntpServers[i]);

    configTime(gmtOffset_sec, daylightOffset_s, ntpServers[i]);
    delay(1000);

    if (getLocalTime(&timeinfo)) {
      DateTime ntpTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

      baseMillis = millis();  // сохраняем базовую метку для millis‑отсчёта
      baseDateTime = ntpTime;

      ntpLastSyncOk = true;
      ntpLastSyncTime = ntpTime.timestamp();
      timeSource = "NTP";

      DateTime rtcTime = getCurrentTime();
      long diff = abs((long)(rtcTime.unixtime() - ntpTime.unixtime()));

      debugLogf("📊 Разница RTC vs NTP: %d сек\n", diff);

      if (diff > 1 && rtcAvailable) {
        rtc.adjust(ntpTime);
        debugLogf("✅ RTC синхронизировано: %02d:%02d:%02d",
                  ntpTime.hour(), ntpTime.minute(), ntpTime.second());
        return ntpTime;
      } else {
        if (rtcAvailable) {
          Serial.println("⏱ RTC уже точное");
        } else {
          Serial.println("⏱ Виртуальное время синхронизировано");
        }
        return rtcTime;
      }
    } else {
      Serial.println("❌ Не удалось получить время");
    }
  }

  Serial.println("⚠️ Все попытки NTP провалились");
  ntpLastSyncOk = false;
  timeSource = rtcAvailable ? "RTC" : "MILLIS";
  Serial.println("⚠️ NTP синхронизация не удалась, часы идут своим ходом");
  return getCurrentTime();
}

// -----------------------------------------------------------------------------
// Ежечасная синхронизация (на 15‑й минуте, если FSM в IDLE)
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
