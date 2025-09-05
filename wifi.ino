// Сетевый функции: подключение к WiFi и синхронизация времени по NTP

void connectToWiFi() {
  const char* ssid = "5stars";
  const char* password = "Vaio8010";
  Serial.print("🔌 Подключаемся к Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(550);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Wi-Fi подключен!");
    Serial.print("📡 IP адрес: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Не удалось подключиться к Wi-Fi! ");
  }
}

DateTime syncRTC() {
  const char* ntpServers[] = {
    "time.belgim.by",
    "1.by.pool.ntp.org",
    "2.by.pool.ntp.org",
    "time.windows.com"
  };
  const int ntpServerCount = sizeof(ntpServers) / sizeof(ntpServers[0]);
  const long gmtOffset_sec = 3 * 3600;
  const int daylightOffset_sec = 0;

  struct tm timeinfo;
  for (int i = 0; i < ntpServerCount; i++) {
    Serial.print("🌐 NTP попытка: ");
    Serial.println(ntpServers[i]);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServers[i]);
    delay(100);

    if (getLocalTime(&timeinfo)) {
      DateTime ntpTime = DateTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

      DateTime rtcTime = rtc.now();
      long diff = abs((long)(rtcTime.unixtime() - ntpTime.unixtime()));

      Serial.print("📊 Разница RTC vs NTP: ");
      Serial.print(diff);
      Serial.println(" сек");

      if (diff > 2) {
        rtc.adjust(ntpTime);
        Serial.print("✅ RTC синхронизировано: ");
        Serial.printf("%02d:%02d:%02d\n", ntpTime.hour(), ntpTime.minute(), ntpTime.second());
        return ntpTime;
      } else {
        Serial.println("⏱ RTC уже точное");
        return rtcTime;  // возвращаем текущее, оно уже точное
      }
    } else {
      Serial.println("❌ Не удалось получить время");
    }
  }

  Serial.println("⚠️ Все попытки NTP провалились");
  return rtc.now();  // возвращаем хоть что-то
}

void handleHourlySync(DateTime now) {  // Синхронизация времени каждый час на 15й минуте в покое
  static int lastHour = -1;
  if (now.hour() != lastHour) {
    lastHour = now.hour();
    syncedThisHour = false;
  }

  if (now.minute() == 15 && !syncedThisHour && arrowState == IDLE) {
    DateTime syncedTime = syncRTC();  // ← вызов синхронизации
    if (syncedTime.isValid()) {       // ← проверка, если нужно
      syncedThisHour = true;
      Serial.println("✅ RTC синхронизировано: " + syncedTime.timestamp());
    } else {
      Serial.println("⚠️ Синхронизация не удалась");
    }
  }
}
