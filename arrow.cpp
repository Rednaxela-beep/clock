// arrow.cpp - Фехтование минутной стрелкой ;-)
#include <Arduino.h>
#include "arrow.h"
#include "main.h"  // доступ к stepper, SET_STATE, IDLE и т.п.
#include "config.h"

// Единственное место, где создаётся переменная состояния
ArrowState arrowState = IDLE;

// Локальные счётчики/состояния конечного автомата
static int lastRtcMinute = -1;
// static int stepCounter = 0;
static ArrowState lastState = IDLE;  // локальная "память" смен состояния

static bool firstLoop = true;  // пропуск первого цикла

// Глобальная метка времени смены состояния
DateTime arrowStateChangedAt;

// Пересчёт параметров при изменении stepFraction
void arrowInitParams() {
  // сколько секунд между стартами (например, 60 при stepFraction=1.0)
  stepIntervalSec   = (int)(60 * stepFraction);

  // время перехода для доли хода
  transitionTimeSec = (int)(baseTransitionSec * stepFraction);

  // защита от вырождения: если вдруг получилось >= интервала
  if (transitionTimeSec >= stepIntervalSec) {
    transitionTimeSec = stepIntervalSec - 1;
  }

  debugLogf("Init params: fraction=%.2f, interval=%d, transition=%d",
            stepFraction, stepIntervalSec, transitionTimeSec);
}
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
  //   if (rtcMinute == 59) { Мониторим точность хода и не останавливаем пока
  //     SET_STATE(IDLE, now);
  //     debugLogf("Концевик на 59-й минуте");
  //     return;
  //   }

    if (rtcMinute >= 27 && rtcMinute <= 29) {
      // SET_STATE(BREAK, now);  // ждём 30-й минуты
      SET_STATE(IDLE, now);
      debugLogf("Второй кулачок на %d-й минуте", rtcMinute);
      return;
    }

    if (rtcMinute >= 50 && rtcMinute <= 58) {  // Пришли в точку 59 раньше
      // SET_STATE(BREAK, now);
      debugLogf("Опережение → стрелка в точке 59, ждём наступления нулевой минуты");
      return;
    }

    if (rtcMinute >= 0 && rtcMinute <= 2) {  // Отставание — нужно догнать 1–3 минуты
      int missedMinutes = rtcMinute + 1;
      int correctionSteps = StepsForMinute * missedMinutes;
      // stepper.moveTo(correctionSteps);
      // SET_STATE(LAG, now);
      debugLogf("Основной кулачок на %d мин", missedMinutes, correctionSteps); // → %d шагов
      return;
    }

    // Всё остальное
    debugLogf("Микрик на %d-й мин.", rtcMinute);
    return;
  }

  // 🎯 Основной конечный автомат
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {  // Сброс, если пошла новая минута
        lastRtcMinute = rtcMinute;
        lastStepSecond = 255;  // разрешаем старт в этой минуте
      }
      // Момент старта: за transitionTimeSec до целевого момента
      if (currentSecond == startSecond && currentSecond != lastStepSecond) {
        lastStepSecond = currentSecond;

        long stepTarget = StepsForMinute * stepFraction;
        stepper.moveTo(stepTarget);

        SET_STATE(MOVING, now);
        Serial.printf("▶️ Предварительный старт: множитель %.2f, интервал %d сек.\n",
              stepFraction, stepIntervalSec);
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
