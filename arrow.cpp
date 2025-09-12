// arrow.cpp - Фехтование минутной стрелкой ;-)
#include <Arduino.h>
#include "arrow.h"
#include "main.h"  // доступ к stepper, SET_STATE, IDLE и т.п.

// Единственное место, где создаётся переменная состояния
ArrowState arrowState = IDLE;

// Локальные счётчики/состояния конечного автомата
static int lastRtcMinute = -1;
static int stepCounter = 0;
static ArrowState lastState = IDLE;  // локальная "память" смен состояния

void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond, bool microSwitchState) {
  // Лог смены состояний (если включено)
  if (arrowState != lastState) {
    lastState = arrowState;
  }

  // 🔄 Лог движения
  if (arrowState == MOVING || arrowState == LAG) {
    if (stepper.distanceToGo() > 0) {
      stepCounter++;
      debugLogf("🦶 Шаг #%d → осталось: %d\n", stepCounter, stepper.distanceToGo());
    }
  }

  // 🐶 Единый сторож микрика в MOVING
  if (arrowState == MOVING && microSwitchState) {
    stepper.setCurrentPosition(0);  // мгновенная остановка
    stepper.disableOutputs();       // отключаем питание

    if (rtcMinute == 59) {
      SET_STATE(IDLE, now);
      debugLogf("✅ Концевик на 59-й минуте → стоп и IDLE");
      return;
    }

    if (rtcMinute >= 27 && rtcMinute <= 29) {
      SET_STATE(BREAK, now);  // ждём 30-й минуты
      debugLogf("⏸ Второй кулачок на %d-й минуте → ждём 30-ю минуту\n", rtcMinute);
      return;
    }

    if (rtcMinute >= 50 && rtcMinute <= 58) {  // Пришли в точку 59 раньше
      SET_STATE(BREAK, now);
      debugLogf("🥊 Опережение → стрелка в точке 59, ждём наступления нулевой минуты");
      return;
    }

    if (rtcMinute >= 0 && rtcMinute <= 2) {  // Отставание — нужно догнать 1–3 минуты
      int missedMinutes = rtcMinute + 1;
      int correctionSteps = StepsForMinute * missedMinutes;
      stepper.moveTo(correctionSteps);
      SET_STATE(LAG, now);
      debugLogf("⏳ LAG: стрелка отстала на %d мин → %d шагов\n", missedMinutes, correctionSteps);
      return;
    }

    // Всё остальное
    debugLogf("🤷 Микрик сработал на %d-й минуте — нужна ручная корректировка\n", rtcMinute);
    return;
  }

  // 🎯 Основной конечный автомат
  switch (arrowState) {
    case IDLE:
      if (rtcMinute != lastRtcMinute) {
        lastRtcMinute = rtcMinute;
        stepper.setCurrentPosition(0);
        stepper.moveTo(StepsForMinute);
        SET_STATE(MOVING, now);
        debugLogf("▶️ Переход %02d\n", rtcMinute);
      }
      break;

    case MOVING:
      if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();  // FSM не решает здесь, только снимаем питание
      }
      break;

    case LAG:
      if (stepper.distanceToGo() == 0) {
        stepper.disableOutputs();
        SET_STATE(IDLE, now);
        debugLogf("✅ LAG завершён — стрелка догнала");
      }
      break;

    case BREAK:
      // Ждём либо начала часа, либо середины
      if (rtcMinute == 0 || rtcMinute == 30) {
        SET_STATE(IDLE, now);
        debugLogf("🕘 BREAK завершён → наступила %02d-я минута, переходим в IDLE\n", rtcMinute);
      }
      break;
  }
}

bool microSw() {
  // Чтобы функция была доступна из других модулей, используем значения из config.h
  // Если у тебя пока в config.h нет этих констант — добавь:
  //   #define MICROSW_PIN D7
  //   #define DEBOUNCE_DELAY 50

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
        debugLogf("🔘 Взвод концевика");
      } else {
        // Срабатывание: кулачок соскакивает
        unsigned long dt = nowMillis - triggerStart;
        if (armed && (dt >= 1000) && (dt <= 300000)) {
          debugLogf("🔘 Концевик сработал!");
          armed = false;
          return true;  // shot!
        } else {
          debugLogf("🕳️ Игнорируем некорректное срабатывание");
          armed = false;
        }
      }
    }
  }
  return false;  // shot не произошёл
}
