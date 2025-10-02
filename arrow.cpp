// arrow.cpp - Фехтование минутной стрелкой ;-)
#include <Arduino.h>
#include "arrow.h"
#include "main.h"  // доступ к stepper, SET_STATE, IDLE и т.п.
#include "config.h"

// Единственное место, где создаётся переменная состояния
ArrowState arrowState = IDLE;

// Локальные счётчики/состояния конечного автомата
int lastRtcMinute = -1;
uint8_t invalidSecond = 255;
static bool correctionApplied = false;

static ArrowState lastState = IDLE;  // локальная "память" смен состояния
static bool firstLoop = true;        // пропуск первого цикла
bool stepperEnabled = false;

// Глобальная метка времени смены состояния
DateTime arrowStateChangedAt;

// -----------------------------------------------------------------------------
// Универсальная функция смены состояния конечного автомата
// -----------------------------------------------------------------------------
void SET_STATE(ArrowState newState, DateTime now) {
  if (arrowState != newState) {
    // *** спец-логика для перехода MOVING → IDLE
    if (arrowState == MOVING && newState == IDLE) {
      stepper.setCurrentPosition(0);  // сбрасываем позицию
      stepper.disableOutputs();       // отключаем питание
      stepperEnabled = false;         // Остановка и обнуление при переходе MOVING → IDLE"
    }
    if (newState == MOVING) {
      correctionApplied = false;  // 🔄 Сброс при новом движении
      stepper.enableOutputs();
      stepperEnabled = true;
    }
    arrowState = newState;
    arrowStateChangedAt = now;  // если ведёшь таймстемп
  }
}
// -----------------------------------------------------------------------------
// Конечный автомат движения и корректировки стрелки
// -----------------------------------------------------------------------------
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchTriggered) {
  static uint8_t invalidSecond = 255;
  static bool firstLoop = true;
  int targetMinute = (rtcMinute + 1) % 60;
  uint8_t startSecond = stepIntervalSec - transitionTimeSec;
  if (startSecond >= stepIntervalSec) startSecond = 0;

  if (microSwitchTriggered && arrowState != MOVING) {
    return;  // ⚠️ Микрик сработал вне движения — игнорируем
  }

  if (firstLoop) {
    lastRtcMinute = rtcMinute;
    firstLoop = false;
    return;
  }

  // 🐶 Сторож микрика в MOVING
  if (arrowState == MOVING && microSwitchTriggered && !correctionApplied) {
    if (targetMinute >= 50 || targetMinute <= 10) {
      correctionApplied = true;

      long current = stepper.currentPosition();
      long newTarget = current;
      int deltaSeconds = 0;

      if (targetMinute == 0) {
        // 🟢 Норма
        newTarget += corrSteps;
        deltaSeconds = round((float)corrSteps / StepsForMinute * 60.0f);
        debugLogf("⏱️ Норма: микрик сработал за ~%d секунд до нуля", deltaSeconds);

      } else if (targetMinute >= 1 && targetMinute <= 10) {
        // 🐢 Отставание
        newTarget += corrSteps + StepsForMinute * targetMinute;
        deltaSeconds = round((float)(corrSteps + StepsForMinute * targetMinute) / StepsForMinute * 60.0f);
        debugLogf("⏱️ Отставание: стрелка отстаёт на ~%d секунд", deltaSeconds);

      } else if (targetMinute >= 50 && targetMinute <= 59) {
        // 🕒 Опережение
        int earlyMinutes = 60 - targetMinute;
        newTarget -= (StepsForMinute * earlyMinutes - corrSteps);
        deltaSeconds = -round((float)(StepsForMinute * earlyMinutes - corrSteps) / StepsForMinute * 60.0f);
        debugLogf("⏱️ Опережение на ~%d секунд", -deltaSeconds);
      }

      // Serial.printf("%02d:%02d:%02d; ⏱️ Коррекция: %s на ~%d секунд\n",
      //               now.hour(), now.minute(), now.second(),
      //               (targetMinute == 0 ? "норма" : targetMinute <= 10 ? "отставание"
      //                                                                 : "опережение"),
      //               abs(deltaSeconds));

      stepper.moveTo(newTarget);
      return;
    }

    return;  // 🔕 Концевик вне интервала — игнор
  }

  // 🎯 Основной конечный автомат
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {
        lastRtcMinute = rtcMinute;
        invalidSecond = 255;
      }

      if ((currentSecond % stepIntervalSec) == startSecond && currentSecond != invalidSecond && !stepper.isRunning()) {

        invalidSecond = currentSecond;
        Serial.printf("%02d:%02d:%02d; ▶️ %02d-й предстарт\n",
                      now.hour(), now.minute(), now.second(),
                      targetMinute);
        stepper.move(StepsForMinute);
        SET_STATE(MOVING, now);
      }
      break;

    case MOVING:
      if (!stepper.isRunning()) {
        SET_STATE(IDLE, now);
      }
      break;
  }
}


// -----------------------------------------------------------------------------
// Обработка срабатывания концевика (edge-triggered + debounce lockout)
// -----------------------------------------------------------------------------
bool microSw() {
  static int lastReading = LOW;
  static int lastStableState = LOW;
  static unsigned long lastDebounce = 0;
  static unsigned long triggerStart = 0;
  static bool armed = false;

  // const unsigned long DEBOUNCE_DELAY = 50;      // дребезг
  const unsigned long MIN_TRIGGER_TIME = 1000;    // минимум между взводом и сработкой
  const unsigned long MAX_TRIGGER_TIME = 300000;  // максимум (5 минут)

  int signal = digitalRead(MICROSW_PIN);
  unsigned long nowMillis = millis();

  // Антидребезг
  if (signal != lastReading) {
    lastDebounce = nowMillis;
    lastReading = signal;
  }

  if ((nowMillis - lastDebounce) > DEBOUNCE_DELAY) {
    if (signal != lastStableState) {
      lastStableState = signal;

      if (signal == HIGH) {
        // Взвод: кулачок наехал
        armed = true;
        triggerStart = nowMillis;
        Serial.println("🔘 Взвод концевика");
      } else {
        // Срабатывание: кулачок соскакивает
        unsigned long dt = nowMillis - triggerStart;
        if (armed && dt >= MIN_TRIGGER_TIME && dt <= MAX_TRIGGER_TIME) {
          long currentStep = stepper.currentPosition();
          float progress = (float)currentStep / StepsForMinute * 100.0f;
          debugLogf("🔘 Концевик сработал! Шаг %ld из %d (%.1f%%)\n", currentStep, StepsForMinute, progress);

          armed = false;
          return true;  // shot!
        } else {
          Serial.printf("🕳️ Игнорируем сработку: Δt = %lu ms\n", dt);
          armed = false;
        }
      }
    }
  }

  return false;  // shot не произошёл
}

// ========== КОНЕЦ arrow.cpp ==========