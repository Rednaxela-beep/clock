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
int correctionDeltaSteps = 0;          // Сколько шагов добавить/отнять
bool applyCorrectionNextStep = false;  // Флаг применения коррекции

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
      stepper.enableOutputs();
      stepperEnabled = true;
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
  static uint8_t invalidSecond = 255;  // 255 — заведомо невозможное значение
  int targetMinute = (rtcMinute + 1) % 60; // Целевая для стрелки минута
  uint8_t startSecond = stepIntervalSec - transitionTimeSec;  // Секунда старта = 60 - transitionTimeSec, но с учётом кратности интервалу
  if (startSecond >= stepIntervalSec) startSecond = 0;        // защита от выхода за диапазон

  if (microSwitchState && arrowState != MOVING) {  // ⚠️ Микрик сработал вне движения — игнорируем
    return;
  }
  if (firstLoop) {
    lastRtcMinute = rtcMinute;  // синхронизируем
    firstLoop = false;
    return;  // пропускаем первый цикл
  }

  // Лог смены состояний (если включено)
  if (arrowState != lastState) {
    lastState = arrowState;
  }

  // 🐶 Единый сторож микрика в MOVING
  if (arrowState == MOVING && microSwitchState) {
    if ((targetMinute >= 50 && targetMinute <= 58) || targetMinute == 59 || (targetMinute >= 0 && targetMinute <= 10)) {  // Подходящие для обработки интервалы
      stepper.stop();                                                                                                     // ⛔️ Останавливаем мотор
      stepper.setCurrentPosition(0);                                                                                      // 🧭 Фиксируем позицию

      int missedMinutes = 0;
      float correctionFactor = 0.0f;
      int correctionSign = 1;

      if (targetMinute >= 50 && targetMinute <= 58) {
        missedMinutes = 60 - targetMinute;
        correctionFactor = missedMinutes - 0.1f;
        correctionSign = -1;
        debugLogf("Опережение: %d мин. Коррекция назад", missedMinutes);
      } else if (targetMinute == 59) {
        correctionDeltaSteps = idealPosition;
        applyCorrectionNextStep = true;
        debugLogf("Норма: микрик сработал на 59-й минуте, стандартная коррекция %d шагов", idealPosition);
        return;
      } else if (targetMinute >= 0 && targetMinute <= 10) {
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
      debugLogf("Микрик вне допустимого интервала — игнор.");
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

        long stepTarget = StepsForMinute;

        if (applyCorrectionNextStep) {
          // debugLogf("🧮 Перед стартом: correctionDeltaSteps = %d, applyCorrectionNextStep = true",
          //           correctionDeltaSteps);

          stepTarget += correctionDeltaSteps;
          applyCorrectionNextStep = false;
          correctionDeltaSteps = 0;

          debugLogf("[%02d:%02d:%02d] ▶️ %02d-й старт: коррекция %+ld шагов\n",
                    now.hour(), now.minute(), now.second(),
                    targetMinute, stepTarget - StepsForMinute);

          debugLogf("🧮 Применяем коррекцию: %+ld шагов", stepTarget - StepsForMinute);
        } else {
          Serial.printf("[%02d:%02d:%02d] ▶️ %02d-й предстарт\n",
                        now.hour(), now.minute(), now.second(),
                        targetMinute);
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
// Обработка срабатывания концевика (edge-triggered + debounce lockout)
bool microSw() {
  static int lastStableState = LOW;
  static unsigned long lastShotTime = 0;
  static unsigned long lastVzvodTime = 0;
  const unsigned long debounceLockout = 1000;
  const unsigned long vzvodLockout = 100;

  int signal = digitalRead(MICROSW_PIN);
  unsigned long nowMillis = millis();

  if (signal != lastStableState) {
    // Обнаружен фронт

    if (signal == LOW && lastStableState == HIGH) {
      // Задний фронт — сработка
      if (nowMillis - lastShotTime > debounceLockout) {
        lastShotTime = nowMillis;
        debugLogf("🔘 Концевик сработал @ %lu ms\n", nowMillis);
        lastStableState = signal;
        return true;
      } else {
        Serial.println("🕳️ Повторное срабатывание заблокировано");
      }

    } else if (signal == HIGH && lastStableState == LOW) {
      // Передний фронт — взвод
      if (nowMillis - lastVzvodTime > vzvodLockout) {
        lastVzvodTime = nowMillis;
        debugLogf("🔘 Взвод концевика @ %lu ms\n", nowMillis);
      } else {
        Serial.println("🕳️ Повторный взвод заблокирован");
      }
    }

    lastStableState = signal;
  }

  return false;
}

// ========== КОНЕЦ arrow.cpp ==========