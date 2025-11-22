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
    arrowStateChangedAt = now;  // таймстемп
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
  if (microSwitchTriggered && arrowState == MOVING) {
    if (targetMinute == 0) {
      // ✅ Норма
      stepper.stop();
      delay(50);
      long correctionSteps = correctionOffset;
      stepper.move(correctionSteps);
      debugLogf("✅ Микрик на нулевой минуте. Стандартная корректировка %ld шагов", correctionSteps);
      // Serial.printf("📐 Двигаем на %ld шагов\n", correctionSteps);
    } else if (targetMinute >= 45 && targetMinute <= 59) {
      // 🕒 Опережение
      stepper.stop();
      delay(50);
      int minutesEarly = 60 - targetMinute;  // Вычисляем к-во минут для корректировки опережения
      long correctionSteps = -StepsForMinute * minutesEarly + correctionOffset;
      stepper.move(correctionSteps);
      debugLogf("🕒 Опережение: возвращаем стрелку на %d минут назад (%ld шагов)", minutesEarly, correctionSteps);
    } else if (targetMinute >= 1 && targetMinute <= 15) {
      // 🐢 Отставание
      long correctionSteps = StepsForMinute * targetMinute + correctionOffset;
      stepper.moveTo(stepper.currentPosition() + correctionSteps);  // без остановки
      debugLogf("🐢 Отставание: продвигаем стрелку на %d минут вперёд (%ld шагов)", targetMinute, correctionSteps);
      // Serial.printf("📐 Двигаем на %ld шагов\n", correctionSteps);
    } else {
      debugLogf("❌ Микрик: минута %d вне допустимого окна корректировки", targetMinute);
    }
    return;
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
        Serial.printf("%02d:%02d:%02d; ▶️ Переход на минуту %02d\n",
                      now.hour(), now.minute(), now.second(), targetMinute);
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
  static int lastSignal = LOW;
  static unsigned long lastDebounceVZVOD = 0;
  static unsigned long lastDebounceSRAB = 0;
  static bool armed = false;
  static unsigned long triggerStart = 0;

  const unsigned long DEBOUNCE_VZVOD = 150;       // дребезг при взводе
  const unsigned long DEBOUNCE_SRAB = 30;         // дребезг при сработке
  const unsigned long MIN_TRIGGER_TIME = 30000;   // минимум 30 секунд
  const unsigned long MAX_TRIGGER_TIME = 300000;  // максимум 5 минут

  int signal = digitalRead(MICROSW_PIN);
  unsigned long nowMillis = millis();

  // Взвод: LOW → HIGH (разрешён в любом состоянии FSM)
  if (signal == HIGH && lastSignal == LOW) {
    lastDebounceVZVOD = nowMillis;
  }
  if (signal == HIGH && (nowMillis - lastDebounceVZVOD) > DEBOUNCE_VZVOD) {
    if (!armed) {
      armed = true;
      triggerStart = nowMillis;
      debugLogf("🔘 Взвод концевика");
    }
  }

  // Сработка: HIGH → LOW (обрабатывается только в MOVING)
  if (signal == LOW && lastSignal == HIGH) {
    lastDebounceSRAB = nowMillis;
  }
  if (signal == LOW && (nowMillis - lastDebounceSRAB) > DEBOUNCE_SRAB) {
    if (armed) {
      unsigned long dt = nowMillis - triggerStart;

      if (arrowState == MOVING) {
        if (dt >= MIN_TRIGGER_TIME && dt <= MAX_TRIGGER_TIME) {
          debugLogf("🔘 Концевик сработал!");
          armed = false;
          lastSignal = signal;
          return true;
        } else {
          Serial.printf("🕳️ Игнорируем сработку: Δt = %lu ms (ручной прогон?)\n", dt);
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
// ========== КОНЕЦ arrow.cpp ==========