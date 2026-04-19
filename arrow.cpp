// arrow.cpp - Фехтование минутной стрелкой ;-)
#include <Arduino.h>
#include "arrow.h"
#include "main.h"
#include "config.h"

// Единственное место, где создаётся переменная состояния
ArrowState arrowState = IDLE;

// Локальные счётчики/состояния конечного автомата
int lastRtcMinute = -1;
static int lastRtcHour = -1;
uint8_t invalidSecond = 255;

static bool correctionThisHour = false;        // была ли уже коррекция в этом часу
static bool zeroTransitionActive = false;      // идём на минуту 00
static bool microTriggeredDuringMove = false;  // микрик сработал во время движения

bool stepperEnabled = false;
long correctionSteps = 0;

DateTime arrowStateChangedAt;

// -----------------------------------------------------------------------------
// Вспомогательные функции
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
      stepper.enableOutputs();
      stepperEnabled = true;
    }

    arrowState = newState;
    arrowStateChangedAt = now;
  }
}

// -----------------------------------------------------------------------------
// Основной конечный автомат
// -----------------------------------------------------------------------------
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchTriggered) {

  static bool firstLoop = true;

  int targetMinute = (lastRtcMinute + 1) % 60;
  uint8_t startSecond = stepIntervalSec - transitionTimeSec;

  if (now.hour() != lastRtcHour) {
    lastRtcHour = now.hour();
    correctionThisHour = false;
  }

  if (startSecond >= stepIntervalSec) startSecond = 0;

  if (firstLoop) {
    lastRtcMinute = rtcMinute;
    firstLoop = false;
    return;
  }

  // фиксируем сработку микрика во время движения
  if (arrowState == MOVING && microSwitchTriggered) {
    microTriggeredDuringMove = true;
  }

  // 🎯 =============== FSM ===============
  switch (arrowState) {

    // ---------------------------------------------------------
    case IDLE:
      if (pendingChimes > 0) {
        int n = pendingChimes;
        pendingChimes = 0;
        hit(n);
        return;
      }

      if (rtcMinute != lastRtcMinute) {
        lastRtcMinute = rtcMinute;
        invalidSecond = 255;
      }

      if ((currentSecond % stepIntervalSec) == startSecond && currentSecond != invalidSecond && !stepper.isRunning()) {

        invalidSecond = currentSecond;

        Serial.printf("%02d:%02d:%02d ▶️ Переход на минуту %02d\n",
                      now.hour(), now.minute(), now.second(), targetMinute);

        zeroTransitionActive = (targetMinute == 0);
        microTriggeredDuringMove = false;

        stepper.moveTo(stepper.currentPosition() + StepsForMinute);
        SET_STATE(MOVING, now);
      }
      break;

    // ---------------------------------------------------------
    case MOVING:

      // движение завершилось
      if (!stepper.isRunning()) {

        Serial.printf("MOVING: стоп, target=%02d, microDuring=%d\n",
                      targetMinute, microTriggeredDuringMove);

        delay(120);  // защита от позднего дребезга

        // микрик НЕ сработал во время движения → отставание
        if (zeroTransitionActive && !microTriggeredDuringMove && !correctionThisHour) {
          Serial.println("⚠️ CORRECT_LAG: микрик НЕ сработал во время движения");
          zeroTransitionActive = false;
          microTriggeredDuringMove = false;
          SET_STATE(CORRECT_LAG, now);
          return;
        }

        zeroTransitionActive = false;
        microTriggeredDuringMove = false;
        SET_STATE(IDLE, now);
              }
      break;

    // ---------------------------------------------------------
    case CORRECT_LAG:
      Serial.println("CORRECT_LAG: начинаем коррекцию отставания");
      {
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

    // ---------------------------------------------------------
    case CORRECT_ADVANCE:
      {
        static bool started = false;

        if (stepper.isRunning()) return;

        if (!started) {
          int minutesEarly = 60 - targetMinute;
          correctionSteps = -StepsForMinute * minutesEarly + correctionOffset;

          Serial.printf("🕒 CORRECT_ADVANCE: возврат на %d минут (%ld шагов)\n",
                        minutesEarly, correctionSteps);

          stepper.move(correctionSteps);
          started = true;
          return;
        }

        started = false;
        correctionThisHour = true;
        SET_STATE(CORRECT_FINE, now);
        return;
      }

    // ---------------------------------------------------------
    case CORRECT_FINE:
      {
        static bool started = false;

        if (stepper.isRunning()) return;

        if (!started) {
          correctionSteps = correctionOffset;

          Serial.printf("🕒 CORRECT_FINE: доводка %ld шагов\n", correctionSteps);

          stepper.move(correctionSteps);
          started = true;
          return;
        }

        started = false;
        correctionThisHour = true;
        SET_STATE(IDLE, now);
                return;
      }
  }
}

// -----------------------------------------------------------------------------
// Микрик
// -----------------------------------------------------------------------------
bool isMovingOrJustStopped() {
  static unsigned long lastMovingTime = 0;

  if (arrowState == MOVING) {
    lastMovingTime = millis();
    return true;
  }

  return (millis() - lastMovingTime) < 200;
}

bool microSw() {  // Микрик с антидребезгом и мгновенной сработкой
  static int lastSignal = LOW;
  static unsigned long vzvodTime = 0;
  static bool armed = false;

  const unsigned long DEBOUNCE_VZVOD = 150;  // дребезг при взводе

  int signal = digitalRead(MICROSW_PIN);
  unsigned long nowMillis = millis();

  // --- ВЗВОД (LOW → HIGH) ---
  if (signal == HIGH && lastSignal == LOW) {
    vzvodTime = nowMillis;
  }

  if (signal == HIGH && (nowMillis - vzvodTime) > DEBOUNCE_VZVOD) {
    if (!armed) {
      armed = true;
      Serial.println("🔘 Взвод концевика");
    }
  }

  // --- СРАБОТКА (HIGH → LOW) ---
  if (signal == LOW && lastSignal == HIGH) {
    if (armed) {
      armed = false;
      Serial.println("🔘 Микрик сработал!");
      lastSignal = signal;
      return true;  // мгновенная реакция
    }
  }

  lastSignal = signal;
  return false;
}


// -----------------------------------------------------------------------------
// FSM → текст
// -----------------------------------------------------------------------------
const char* fsmToText(ArrowState s, int targetMinute, int pendingChimes) {
  static char buf[64];

  switch (s) {
    case IDLE:
      if (pendingChimes > 0) {
        snprintf(buf, sizeof(buf), "Бой %d раз", pendingChimes);
        return buf;
      }
      return "Ожидание";

    case MOVING:
      snprintf(buf, sizeof(buf), "Переход на минуту %02d", targetMinute);
      return buf;

    case CORRECT_LAG:
      return "Коррекция отставания";

    case CORRECT_ADVANCE:
      return "Коррекция опережения";

    case CORRECT_FINE:
      return "Точная доводка";

    default:
      return "Неизвестно";
  }
}
// Конец arrow.cpp