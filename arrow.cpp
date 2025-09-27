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
// Универсальная функция смены состояния FSM
// -----------------------------------------------------------------------------
void SET_STATE(ArrowState newState, DateTime now) {
  if (arrowState != newState) {
    // *** спец-логика для перехода MOVING → IDLE
    if (arrowState == MOVING && newState == IDLE) {
      stepper.setCurrentPosition(0);  // сбрасываем позицию
      stepper.disableOutputs();       // отключаем питание
      // debugLogf("Остановка и обнуление при переходе MOVING → IDLE");
    }

    Serial.printf("[%02d:%02d:%02d] ⚙️ FSM: %s → %s\n",
                  now.hour(), now.minute(), now.second(),
                  stateName(arrowState), stateName(newState));

    arrowState = newState;
    arrowStateChangedAt = now;  // если ведёшь таймстемп
  }
}
// -----------------------------------------------------------------------------
// Конечный автомат движения и корректировки стрелки
// -----------------------------------------------------------------------------
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchState) {
  static uint8_t lastStepSecond = 255;                        // 255 — заведомо невозможное значение
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

  // 🐶 Единый сторож микрика в MOVING
  if (arrowState == MOVING && microSwitchState) {

    if (rtcMinute >= 20 && rtcMinute <= 40) {  // Только лог для контроля
      debugLogf("Второй кулачок сработал");
      return;
    }

    if (rtcMinute >= 55 || rtcMinute <= 05) {  // Корректируем струлку если нужно
      int idealSecond = 59 * 60 + 58;          // 59 мин 58 сек = 3598
      int actualSecond = rtcMinute * 60 + currentSecond;
      int deltaSec = actualSecond - idealSecond;

      // округление до ближайших 10 сек, если дельта ≥ 6 сек
      int roundedDeltaSec = (abs(deltaSec) >= 6) ? (deltaSec / 10) * 10 : 0;

      if (roundedDeltaSec != 0) {
        correctionDeltaSteps = (roundedDeltaSec * StepsForMinute) / 60;
        applyCorrectionNextStep = true;

        debugLogf("Дельта %d сек. Корректировка %d шагов",
                  deltaSec, correctionDeltaSteps);
      } else {
        debugLogf("Дельта %d сек. Корректировка не требуется", deltaSec);
      }
      return;
    }
  }
  // 🎯 Основной конечный автомат
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {  // Сброс, если пошла новая минута
        lastRtcMinute = rtcMinute;
        lastStepSecond = 255;  // разрешаем старт в этой минуте
      }
      // Секунда старта = (stepIntervalSec - transitionTimeSec)
      startSecond = (stepIntervalSec - transitionTimeSec) % stepIntervalSec;

      if ((currentSecond % stepIntervalSec) == startSecond && currentSecond != lastStepSecond && !stepper.isRunning()) {

        lastStepSecond = currentSecond;

        long stepTarget = StepsForMinute;

        if (applyCorrectionNextStep) {  // Собственно корректировка
          stepTarget += correctionDeltaSteps;
          applyCorrectionNextStep = false;
          correctionDeltaSteps = 0;

          Serial.printf("▶️ %02d предварительный старт: коррекция %+ld шагов\n",
                        (rtcMinute + 1) % 60, stepTarget - StepsForMinute);
        } else {
          Serial.printf("▶️ %02d-й предварительный старт\n",
                        (rtcMinute + 1) % 60);
        }

        stepper.move(stepTarget);
        SET_STATE(MOVING, now);

        Serial.printf("▶️ %02d предварительный старт \n",
                      (rtcMinute + 1) % 60);
      }
      break;

    case MOVING:
      if (!stepper.isRunning()) {
        SET_STATE(IDLE, now);
      }
      break;

    case LAG:
      if (stepper.distanceToGo() == 0) {
        SET_STATE(IDLE, now);
        Serial.printf("LAG завершён — стрелка догнала");
      }
      break;

    case BREAK:
      if (rtcMinute == 0 || rtcMinute == 30) {  // Ждём либо начала часа, либо середины
        SET_STATE(IDLE, now);
        debugLogf("BREAK завершён → наступила %02d-я минута, переходим в IDLE\n", rtcMinute);
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
        if (armed && (dt >= 1000) && (dt <= 300000)) {
          //          Serial.printf("🔘 Концевик сработал!");
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
