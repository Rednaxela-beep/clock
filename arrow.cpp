// arrow.cpp - Фехтование минутной стрелкой ;-)
#include <Arduino.h>
#include "arrow.h"
#include "main.h"  // доступ к stepper, SET_STATE, IDLE и т.п.
#include "config.h"

// Единственное место, где создаётся переменная состояния
ArrowState arrowState = IDLE;

// Локальные счётчики/состояния конечного автомата
int lastRtcMinute = -1;
static int lastRtcHour = -1;
uint8_t invalidSecond = 255;
static bool correctionApplied = false;
static bool correctionThisHour = false;  // была ли уже коррекция (любая) в этом часу

static ArrowState lastState = IDLE;  // локальная "память" смен состояния
static bool firstLoop = true;        // пропуск первого цикла
bool stepperEnabled = false;
long correctionSteps = 0;  // Значение корректировки глобально для JSON

DateTime arrowStateChangedAt;  // Глобальная метка времени смены состояния

// -----------------------------------------------------------------------------
// Универсальная функция смены состояния конечного автомата
// -----------------------------------------------------------------------------
bool isMotionState(ArrowState s) {
  return (s == MOVING || s == CORRECT_LAG || s == CORRECT_ADVANCE || s == CORRECT_FINE);
}

void SET_STATE(ArrowState newState, DateTime now) {
  if (arrowState != newState) {

    if (isMotionState(arrowState) && newState == IDLE) {
      stepper.setCurrentPosition(0);
      stepper.disableOutputs();
      stepperEnabled = false;
    }

    if (!isMotionState(arrowState) && isMotionState(newState)) {
      correctionApplied = false;
      stepper.enableOutputs();
      stepperEnabled = true;
    }

    arrowState = newState;
    arrowStateChangedAt = now;
  }
}

// -----------------------------------------------------------------------------
// Конечный автомат движения и корректировки стрелки
// -----------------------------------------------------------------------------
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchTriggered) {

  static bool firstLoop = true;
  int targetMinute = (rtcMinute + 1) % 60;
  uint8_t startSecond = stepIntervalSec - transitionTimeSec;

  if (now.hour() != lastRtcHour) {
    lastRtcHour = now.hour();
    correctionThisHour = false;
  }

  if (startSecond >= stepIntervalSec) startSecond = 0;

  if (microSwitchTriggered && arrowState != MOVING) {
    return;
  }

  if (firstLoop) {
    lastRtcMinute = rtcMinute;
    firstLoop = false;
    return;
  }

  // 🐶 Сторож микрика в MOVING
  if (microSwitchTriggered && arrowState == MOVING) {

    // Опережение
    if (targetMinute >= 45 && targetMinute <= 59 && !correctionThisHour) {
      SET_STATE(CORRECT_ADVANCE, now);
      return;
    }

    debugLogf("targetMinute=%d; stepperPos=%ld",
              targetMinute, stepper.currentPosition());
  }

  // 🎯 =============== Основной конечный автомат ===============
  switch (arrowState) {

    case IDLE:
      if (rtcMinute != lastRtcMinute) {
        lastRtcMinute = rtcMinute;
        invalidSecond = 255;
      }

      if ((currentSecond % stepIntervalSec) == startSecond &&
          currentSecond != invalidSecond &&
          !stepper.isRunning()) {

        invalidSecond = currentSecond;

        Serial.printf("%02d:%02d:%02d; ▶️ Переход на минуту %02d\n",
                      now.hour(), now.minute(), now.second(), targetMinute);

        stepper.move(stepper.currentPosition() + StepsForMinute);
        SET_STATE(MOVING, now);
      }
      break;

    case MOVING:
      if (!stepper.isRunning()) {

        delay(120);

        if (targetMinute == 0 &&
            !microSwitchTriggered &&
            !correctionThisHour) {

          SET_STATE(CORRECT_LAG, now);
          return;
        }

        SET_STATE(IDLE, now);
      }
      break;

    case CORRECT_LAG: {
      static long lagSteps = 0;

      if (stepper.isRunning()) return;

      if (microSwitchTriggered) {
        correctionThisHour = true;
        lagSteps = 0;
        SET_STATE(CORRECT_FINE, now);
        return;
      }

      if (lagSteps > StepsForMinute * 15) {
        Serial.println("❌ ERROR: Lag correction exceeded 15 minutes!");
        SET_STATE(IDLE, now);
        return;
      }

      stepper.move(StepsForMinute);
      lagSteps += StepsForMinute;
      return;
    }
    break;

    case CORRECT_ADVANCE: {
      static bool started = false;

      if (stepper.isRunning()) return;

      if (!started) {
        int minutesEarly = 60 - targetMinute;
        correctionSteps = -StepsForMinute * minutesEarly + correctionOffset;

        debugLogf("🕒 CORRECT_ADVANCE: возврат на %d минут (%ld шагов)",
                  minutesEarly, correctionSteps);

        stepper.move(correctionSteps);
        started = true;
        return;
      }

      if (!stepper.isRunning()) {
        started = false;
        correctionThisHour = true;
        SET_STATE(CORRECT_FINE, now);
      }
      return;
    }
    break;

    case CORRECT_FINE: {
      static bool started = false;

      if (stepper.isRunning()) return;

      if (!started) {
        correctionSteps = correctionOffset;

        debugLogf("🕒 CORRECT_FINE: доводка %ld шагов", correctionSteps);

        stepper.move(correctionSteps);
        started = true;
        return;
      }

      if (!stepper.isRunning()) {
        started = false;
        correctionThisHour = true;
        SET_STATE(IDLE, now);
      }
      return;
    }
    break;
  }
}

// -----------------------------------------------------------------------------
// Функция: считаем, что стрелка "ещё движется" 200 мс после выхода из MOVING
// -----------------------------------------------------------------------------
bool isMovingOrJustStopped() {
  static unsigned long lastMovingTime = 0;

  if (arrowState == MOVING) {
    lastMovingTime = millis();
    return true;
  }

  return (millis() - lastMovingTime) < 200;
}

// -----------------------------------------------------------------------------
// Обработка срабатывания концевика
// -----------------------------------------------------------------------------
bool microSw() {
  static int lastSignal = LOW;
  static unsigned long lastDebounceVZVOD = 0;
  static unsigned long lastDebounceSRAB = 0;
  static bool armed = false;
  static unsigned long triggerStart = 0;

  const unsigned long DEBOUNCE_VZVOD = 150;
  const unsigned long DEBOUNCE_SRAB = 30;
  const unsigned long MIN_TRIGGER_TIME = 30000;
  const unsigned long MAX_TRIGGER_TIME = 300000;

  int signal = digitalRead(MICROSW_PIN);
  unsigned long nowMillis = millis();

  if (signal == HIGH && lastSignal == LOW) {
    lastDebounceVZVOD = nowMillis;
  }
  if (signal == HIGH && (nowMillis - lastDebounceVZVOD) > DEBOUNCE_VZVOD) {
    if (!armed) {
      armed = true;
      triggerStart = nowMillis;
      Serial.printf("🔘 Взвод концевика");
    }
  }

  if (signal == LOW && lastSignal == HIGH) {
    lastDebounceSRAB = nowMillis;
  }
  if (signal == LOW && (nowMillis - lastDebounceSRAB) > DEBOUNCE_SRAB) {
    if (armed) {
      unsigned long dt = nowMillis - triggerStart;

      if (isMovingOrJustStopped()) {
        if (dt >= MIN_TRIGGER_TIME && dt <= MAX_TRIGGER_TIME) {
          Serial.printf("🔘 Концевик сработал!");
          armed = false;
          lastSignal = signal;
          return true;
        } else {
          Serial.printf("🕳️ Игнорируем сработку: Δt = %lu ms\n", dt);
          armed = false;
        }
      } else {
        Serial.printf("🕳️ Сработал вне MOVING — игнорируем. Δt = %lu ms\n", dt);
        armed = false;
      }
    }
  }

  lastSignal = signal;
  return false;
}
// Конец arrow.cpp