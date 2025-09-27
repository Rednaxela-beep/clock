// arrow.cpp - Фехтование минутной стрелкой ;-)
#include <Arduino.h>
#include "arrow.h"
#include "main.h"  // доступ к stepper, SET_STATE, IDLE и т.п.
#include "config.h"

// Единственное место, где создаётся переменная состояния
ArrowState arrowState = IDLE;

// Локальные счётчики/состояния конечного автомата
static int lastRtcMinute = -1;
static int correctionDeltaSteps = 0;          // Сколько шагов добавить/отнять
static bool applyCorrectionNextStep = false;  // Флаг применения коррекции

static ArrowState lastState = IDLE;  // локальная "память" смен состояния

static bool firstLoop = true;  // пропуск первого цикла

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
      // debugLogf("Остановка и обнуление при переходе MOVING → IDLE");
    }

    // Serial.printf("[%02d:%02d:%02d] ⚙️ FSM: %s → %s\n",
    //               now.hour(), now.minute(), now.second(),
    //               stateName(arrowState), stateName(newState));

    arrowState = newState;
    arrowStateChangedAt = now;  // если ведёшь таймстемп
  }
}
// -----------------------------------------------------------------------------
// Конечный автомат движения и корректировки стрелки
// -----------------------------------------------------------------------------
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchState) {
  static uint8_t invalidSecond = 255;                         // 255 — заведомо невозможное значение
  uint8_t startSecond = stepIntervalSec - transitionTimeSec;  // Секунда старта = 60 - transitionTimeSec, но с учётом кратности интервалу
  if (startSecond >= stepIntervalSec) startSecond = 0;        // защита от выхода за диапазон


  if (firstLoop) {
    lastRtcMinute = rtcMinute;  // синхронизируем
    firstLoop = false;
    return;  // пропускаем первый цикл
  }

  // Лог смены состояний (если включено)
  if (arrowState != lastState) {
    lastState = arrowState;
  }

  stepper.stop();                 // ⛔️ Останавливаем мотор
  stepper.setCurrentPosition(0);  // 🧭 Фиксируем позицию

  // 🐶 Единый сторож микрика в MOVING
  if (arrowState == MOVING && microSwitchState) {
    if (rtcMinute >= 56 && rtcMinute <= 58 || rtcMinute == 59 || rtcMinute >= 0 && rtcMinute <= 5) {
      stepper.stop();
      stepper.setCurrentPosition(0);

      int missedMinutes = 0;
      float correctionFactor = 0.0f;
      int correctionSign = 1;

      if (rtcMinute >= 56 && rtcMinute <= 58) {
        missedMinutes = 60 - rtcMinute;
        correctionFactor = missedMinutes - 0.1f;
        correctionSign = -1;
        debugLogf("Опережение: %d мин. Коррекция назад", missedMinutes);
      } else if (rtcMinute == 59) {
        correctionDeltaSteps = idealPosition;
        applyCorrectionNextStep = true;
        debugLogf("Норма: микрик сработал на 59-й минуте, коррекция положения %d шагов", idealPosition);
        return;
      } else if (rtcMinute >= 0 && rtcMinute <= 5) {
        missedMinutes = rtcMinute + 1;
        correctionFactor = missedMinutes - 0.1f;
        correctionSign = +1;
        debugLogf("Отставание: %d мин. Коррекция вперёд", missedMinutes);
      }

      correctionDeltaSteps = correctionSign * (int)(abs(StepsForMinute) * correctionFactor);
      applyCorrectionNextStep = true;
      debugLogf("✅ Коррекция рассчитана: %d шагов, applyCorrectionNextStep = %s",
                correctionDeltaSteps, applyCorrectionNextStep ? "true" : "false");
      debugLogf("Коррекция: %+d шагов", correctionDeltaSteps);
      return;
    } else {
      debugLogf("Микрик вне допустимого интервала — игнор");
      return;
    }
  }

  // 🎯 Основной конечный автомат
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {  // Сброс, если пошла новая минута
        lastRtcMinute = rtcMinute;
        invalidSecond = 255;  // разрешаем старт в этой минуте
      }

      if ((currentSecond % stepIntervalSec) == startSecond && currentSecond != invalidSecond && !stepper.isRunning()) {

        invalidSecond = currentSecond;

        long stepTarget = StepsForMinute;  // Собственно корректировка
        if (applyCorrectionNextStep) {
          stepTarget += correctionDeltaSteps;
          applyCorrectionNextStep = false;
          correctionDeltaSteps = 0;

          Serial.printf("[%02d:%02d:%02d] ▶️ %02d-й старт: коррекция %+ld шагов\n",
                        now.hour(), now.minute(), now.second(),
                        (rtcMinute + 1) % 60, stepTarget - StepsForMinute);

        } else {
          Serial.printf("[%02d:%02d:%02d] ▶️ %02d-й старт\n",
                        now.hour(), now.minute(), now.second(),
                        (rtcMinute + 1) % 60);
        }
        if (applyCorrectionNextStep) {
          debugLogf("🧮 Применяем коррекцию: %+d шагов", correctionDeltaSteps);
        }

        stepper.move(stepTarget);
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
// Обработка срабатывания концевика (микрика)
// -----------------------------------------------------------------------------
bool microSw() {
  static int lastReading = LOW;
  static int lastStableState = LOW;
  static unsigned long lastDebounce = 0;
  static unsigned long triggerStart = 0;
  static bool armed = false;

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
        if (armed && (dt >= 1000) && (dt <= 300000)) {  // 🔘 Концевик сработал!
          Serial.println("🔘 Концевик сработал — фиксируем положение");
          armed = false;
          return true;  // shot!
        } else {
          Serial.printf("🕳️ Игнорируем некорректное срабатывание");
          armed = false;
        }
      }
    }
  }
  return false;  // shot не произошёл
}
// ========== КОНЕЦ arrow.cpp ==========