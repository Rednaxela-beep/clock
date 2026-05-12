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
float lastCorrectionMinutes = NAN;  // Последняя коррекция. NAN = не было коррекции

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
  static int microTriggerMinute = -1;

  int targetMinute = (lastRtcMinute + 1) % 60;
  uint8_t startSecond = stepIntervalSec - transitionTimeSec;


  if (startSecond >= stepIntervalSec) startSecond = 0;

  if (firstLoop) {
    lastRtcMinute = rtcMinute;
    firstLoop = false;
    return;
  }
  // фиксируем сработку микрика во время движения
  if (arrowState == MOVING && microSwitchTriggered) {
    microTriggeredDuringMove = true;
    microTriggerMinute = targetMinute;  // фиксируем минуту срабатывания
  }

  // 🎯 =============== FSM ===============
  switch (arrowState) {
    case IDLE:  // ----------- Ожидание -----------
      if (pendingChimes > 0) {
        int n = pendingChimes;
        pendingChimes = 0;
        hit(n);
        return;
      }

      if (rtcMinute != lastRtcMinute) {
        // если была коррекция опережения — сбрасываем флаг на минуте 01
        if (rtcMinute == 1 && correctionThisHour) {
          Serial.println("Сбрасываем correctionThisHour на минуте 01");
          correctionThisHour = false;
        }
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
    case MOVING:                   // ----------- Движение -----------
      if (!stepper.isRunning()) {  // движение завершилось
        // --- ОПЕРЕЖЕНИЕ (микрик сработал ДО нулевой минуты) ---
        if (microTriggeredDuringMove && microTriggerMinute >= 45 && microTriggerMinute <= 59) {

          Serial.printf("⚡ Опережение: микрик сработал на минуте %02d\n",
                        microTriggerMinute);
          microTriggeredDuringMove = false;
          SET_STATE(CORRECT_ADVANCE, now);
          return;
        }
        // --- НОРМАЛЬНЫЙ НОЛЬ ПОСЛЕ коррекции опережения ---
        if (zeroTransitionActive && !microTriggeredDuringMove && correctionThisHour) {
          Serial.println("Нормальный ноль после коррекции опережения");
          zeroTransitionActive = false;
          microTriggeredDuringMove = false;
          // correctionThisHour НЕ трогаем здесь — он уйдёт на минуте 01
          SET_STATE(IDLE, now);
          return;
        }
        // --- ОТСТАВАНИЕ ---
        if (zeroTransitionActive && !microTriggeredDuringMove && !correctionThisHour) {
          Serial.println("⚠️ Отставание: микрик НЕ сработал во время Перехода на минуту 00");
          zeroTransitionActive = false;
          microTriggeredDuringMove = false;
          SET_STATE(CORRECT_LAG, now);
          return;
        }
        // --- ИДЕАЛЬНЫЙ НОЛЬ (микрик сработал ровно на минуте 00) ---
        if (zeroTransitionActive && microTriggeredDuringMove && microTriggerMinute == 0) {
          SET_STATE(CORRECT_FINE, now);
          return;
        }
        // обычное завершение движения
        zeroTransitionActive = false;
        microTriggeredDuringMove = false;
        SET_STATE(IDLE, now);
      }
      break;

      // ------------------ Коррекция отставания ---------------------------------------
    case CORRECT_LAG:  // ----------- Коррекция отставания -----------
      {
        static bool started = false;
        static long targetSteps = 0;

        // 1) Ловим микрик во время движения
        if (stepper.isRunning()) {

          if (microSwitchTriggered) {
            Serial.println("Коррекция отставания: микрик сработал — здесь СТОП!");
            float movedSteps = stepper.currentPosition();  // сколько реально проехали
            lastCorrectionMinutes = movedSteps / (float)StepsForMinute;

            stepper.stop();  // попросить остановиться
            while (stepper.isRunning()) {
              stepper.run();  // дожать до реального стопа
            }
            stepper.disableOutputs();  // сбросить фазы
            delay(5);                  // короткая пауза
            stepper.enableOutputs();   // включить снова

            stepper.setCurrentPosition(0);  // зафиксировать точку
            started = false;
            microTriggeredDuringMove = false;
            correctionThisHour = false;  // после лаг-коррекции мы уже в новом часе
            SET_STATE(IDLE, now);
          }
          return;
        }

        // 2) Первый вход
        if (!started) {
          Serial.println("Коррекция отставания: начинаем коррекцию");

          targetSteps = StepsForMinute * 15;
          microTriggeredDuringMove = false;

          stepper.move(targetSteps);
          started = true;
          return;
        }

        // 3) Движение завершилось, но микрик НЕ сработал
        Serial.println("❌ ERROR: Lag correction exceeded 15 minutes!");
        started = false;
        microTriggeredDuringMove = false;
        SET_STATE(IDLE, now);
        return;
      }
      break;
    case CORRECT_ADVANCE:  // ----------- Коррекция опережения -----------
      {
        static bool started = false;
        static long targetSteps = 0;

        // 1) Если шаговик ещё крутится — ждём
        if (stepper.isRunning()) {
          return;
        }

        // 2) Первый вход
        if (!started) {

          int advanceMinutes = 60 - microTriggerMinute;

          if (advanceMinutes <= 0 || advanceMinutes > 15) {
            advanceMinutes = 1;  // защита
          }

          targetSteps = -(advanceMinutes * StepsForMinute);

          Serial.printf("Коррекция опережения: откат %d мин (%ld шагов)\n",
                        advanceMinutes, targetSteps);

          stepper.move(targetSteps);
          started = true;
          return;
        }

        // 3) Движение завершилось
        stepper.setCurrentPosition(0);

        correctionThisHour = true;  // важно: чтобы не ловить ложный lag на минуте 00
        microTriggerMinute = -1;

        started = false;

        Serial.println("Коррекция опережения завершена");
        lastCorrectionMinutes = -(60.0f - microTriggerMinute);
        SET_STATE(IDLE, now);
        return;
      }
    case CORRECT_FINE:
      {
        Serial.println("CORRECT_FINE: точная доводка (пока не требуется)");
        // Возможно в будущем здесь будет компенсация
        lastCorrectionMinutes = 0.0f;
        SET_STATE(IDLE, now);
        return;
      }
      break;
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
// Преобразование состояний FSM в текст для отправки в монитор
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