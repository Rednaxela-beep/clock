// arrow.ino - Фехтование минутной стрелкой ;-)
int StepsForMinute = -6500;  // Одна минута в полушаговом режиме = теоритически 6245 шагов

void logFSM(DateTime now, ArrowState state) {  // Функция логирования состояния FSM
  const char* name = nullptr;
  switch (state) {
    case IDLE: name = "IDLE"; break;
    case MOVING: name = "MOVING"; break;
    case LAG: name = "CORRECTING_LAG"; break;
    case BREAK: name = "WAITING_FOR_ZERO"; break;
    default: name = "UNKNOWN"; break;
  }
  Serial.printf("[%02d:%02d:%02d] ↩️ FSM: %s\n", now.hour(), now.minute(), now.second(), name);
}

// Довольно неглупая FSM со сторожами микрика времена для цирка со стрелкой
void arrowFSM_update(DateTime now, int arrowMinute, int rtcMinute, int currentSecond, bool microSwitchState) {
  static int lastRtcMinute = -1;
  static ArrowState lastState = UNKNOWN;
  static int stepCounter = 0;

  if (arrowState != lastState) {
    logFSM(now, arrowState);
    lastState = arrowState;
  }

  if (arrowState == MOVING || arrowState == LAG) {
    if (stepper.distanceToGo() > 0) {
      stepCounter++;
      Serial.printf("🦶 Шаг #%d → осталось: %d\n", stepCounter, stepper.distanceToGo());
    }
  }

  // 🐶 Сторож №1 — реагирует на микрик в MOVING
  if (arrowState == MOVING && microSwitchState) {
    if (rtcMinute == 59) {
      stepper.disableOutputs();
      arrowState = IDLE;
      Serial.println("✅ Концевик на 59-й минуте → стоп и IDLE");
      return;
    } else if (rtcMinute >= 45 && rtcMinute <= 58) {
      stepper.disableOutputs();
      arrowState = BREAK;
      Serial.println("🥊 BREAK: опережение → ждём 00:00");
      return;
    }
  }

  // 🐶 Сторож №2 — реагирует на микрик в начале часа
  if (arrowState == IDLE && rtcMinute >= 0 && rtcMinute <= 3 && microSwitchState) {
    int missedMinutes = rtcMinute;
    int remainingSteps = StepsForMinute - stepper.currentPosition();
    int correctionSteps = remainingSteps * missedMinutes;

    stepper.setCurrentPosition(0);
    stepper.moveTo(correctionSteps);
    arrowState = LAG;
    Serial.printf("⏳ Корректировка отставания: %d шагов\n", correctionSteps);
    return;
  }
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {
        lastRtcMinute = rtcMinute;
        if (arrowMinute != rtcMinute) {
          stepper.setCurrentPosition(0);
          stepper.moveTo(StepsForMinute);
          arrowState = MOVING;
          Serial.printf("▶️ Переход на минуту %02d\n", rtcMinute);
        }
      }
      break;

    case MOVING:
      if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();

        if (arrowMinute == 59 && !microSwitchState) {
          stepper.setCurrentPosition(0);
          stepper.moveTo(StepsForMinute * 4);
          arrowState = LAG;
          Serial.println("⚠️ Корректировка на 59-й минуте → начинаем движение до микрика");
        } else {
          arrowState = IDLE;
        }
      }
      break;

    case LAG:
      if (microSwTriggered) {
        microSwTriggered = false;
        stepper.disableOutputs();
        arrowState = IDLE;
        Serial.println("✅ Корректировка завершена по микрику");
      } else if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();
        arrowState = IDLE;
        Serial.println("⚠️ Движение завершено, но микрик не сработал");
      }
      break;

    case BREAK:
      if (rtcMinute == 0 && currentSecond <= 5) {
        stepper.setCurrentPosition(0);
        stepper.moveTo(StepsForMinute);
        arrowState = MOVING;
        Serial.println("🕛 00:00 → продолжаем движение");
      }
      break;
  }
}

void microSw() {
  int signal = digitalRead(microSw_PIN);
  static int lastSignalMinute = -1;

  if (signal != lastReading) {
    lastDebounceTime = millis();
    lastReading = signal;
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (signal != lastStableState) {
      lastStableState = signal;

      if (signal == HIGH) {
        Serial.println("🔘 Взвод концевика");
      } else {
        Serial.println("🔘 Концевик сработал!");

        // Фильтрация по минуте
        DateTime now = rtc.now();  // ← если rtc доступен глобально
        int m = now.minute();

        // if (m == 59) {
        //   microSwTriggered = true;
        // } else {
        //   Serial.println("🕳️ Игнорируем срабатывание вне 59-й минуты");
        //   microSwTriggered = false;
        // }
      }
    }
  }
}
