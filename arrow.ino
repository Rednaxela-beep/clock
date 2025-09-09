// arrow.ino - Фехтование минутной стрелкой ;-)
int StepsForMinute = -6672;  // Одна минута в полушаговом режиме = теоритически 6245 шагов

void logFSM(DateTime now, ArrowState state) {
  const char* name = nullptr;
  switch (state) {
    case IDLE:   name = "IDLE"; break;
    case MOVING: name = "MOVING"; break;
    case LAG:    name = "CORRECTING_LAG"; break;
    case BREAK:  name = "WAITING_FOR_ZERO"; break;
  }
  Serial.printf("[%02d:%02d:%02d] ↩️ FSM: %s\n", now.hour(), now.minute(), now.second(), name);
}

void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchState) {
  static int lastRtcMinute = -1;
  static int stepCounter = 0;
  static ArrowState lastState = IDLE;

  if (arrowState != lastState) {
    logFSM(now, arrowState);
    lastState = arrowState;
  }

  // 🔄 Лог движения
  if (arrowState == MOVING || arrowState == LAG) {
    if (stepper.distanceToGo() > 0) {
      stepCounter++;
      Serial.printf("🦶 Шаг #%d → осталось: %d\n", stepCounter, stepper.distanceToGo());
    }
  }

  // 🐶 Единый сторож микрика в MOVING
  if (arrowState == MOVING && microSwitchState) {
    stepper.setCurrentPosition(0); // мгновенная остановка
    stepper.disableOutputs();      // отключаем питание

    if (rtcMinute == 59) {
      arrowState = IDLE;
      Serial.println("✅ Концевик на 59-й минуте → стоп и IDLE");
      return;
    }
 
    if (rtcMinute == 29) { // Нормальное срабатывание на 29-й минуте (второй кулачок)
        arrowState = IDLE;
        Serial.println("✅ Концевик на 29-й минуте → стоп и IDLE");
        return;
    }

    if (rtcMinute >= 50 && rtcMinute <= 58) { // Пришли в точку 59 раньше
      arrowSt66ate = BREAK;
      Serial.println("🥊 Опережение → стрелка в точке 59, ждём наступления 59-й минуты");
      return;
    }

    if (rtcMinute >= 0 && rtcMinute <= 2) { // Отставание - нужно догнать от 1 до 3х минут
      int missedMinutes = rtcMinute + 1;
      int correctionSteps = StepsForMinute * missedMinutes;
      stepper.moveTo(correctionSteps);
      arrowState = LAG;
      Serial.printf("⏳ LAG: стрелка отстала на %d мин → %d шагов\n", missedMinutes, correctionSteps);
      return;
    }

    // Всё остальное
    Serial.printf("🤷 Микрик сработал на %d-й минуте — нужна ручная корректировка\n", rtcMinute);
    return;
  }

  // 🎯 Основной FSM
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {
        lastRtcMinute = rtcMinute;
        stepper.setCurrentPosition(0);
        stepper.moveTo(StepsForMinute);
        arrowState = MOVING;
        Serial.printf("▶️ Переход на минуту %02d\n", rtcMinute);
      }
      break;

    case MOVING:
      if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();  // просто отключаем, FSM не решает здесь
      }
      break;

    case LAG:
      if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();
        arrowState = IDLE;
        Serial.println("✅ LAG завершён — стрелка догнала");
      }
      break;

    case BREAK:
      if (rtcMinute == 59) {
        arrowState = IDLE;
        Serial.println("🕘 BREAK завершён → наступила 59-я минута, переходим в IDLE");
      }
      break;
  }
}


bool microSw() {
  static int lastReading = LOW;
  static int lastStableState = LOW;
  static unsigned long lastDebounceTime = 0;
  static unsigned long triggerWindowStart = 0;
  static bool armed = false;

  int signal = digitalRead(microSw_PIN);
  unsigned long nowMillis = millis();

  // Антидребезг
  if (signal != lastReading) {
    lastDebounceTime = nowMillis;
    lastReading = signal;
  }

  if ((nowMillis - lastDebounceTime) > debounceDelay) {
    if (signal != lastStableState) {
      lastStableState = signal;

      if (signal == HIGH) {
        // Взвод: кулачок наехал
        armed = true;
        triggerWindowStart = nowMillis;
        Serial.println("🔘 Взвод концевика");
      } else {
        // Срабатывание: кулачок соскакивает
        if (armed && (nowMillis - triggerWindowStart >= 1000) && (nowMillis - triggerWindowStart <= 300000)) {
          Serial.println("🔘 Концевик сработал!");
          armed = false;
          return true;  // shot!
        } else {
          Serial.println("🕳️ Игнорируем некорректное срабатывание");
          armed = false;
        }
      }
    }
  }
  return false;  // shot не произошёл
}