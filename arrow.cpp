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
void arrowFSM_update(DateTime now, int rtcMinute, int currentSecond) {

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

    case MOVING:  // ----------- Движение -----------
      stepper.run();
      if (microSw()) {
        microTriggeredDuringMove = true;
        microTriggerMinute = targetMinute;
      }

      if (!stepper.isRunning()) {  // движение завершилось
        // --- ОПЕРЕЖЕНИЕ (микрик сработал ДО нулевой минуты) ---
        if (microTriggeredDuringMove && microTriggerMinute >= 45 && microTriggerMinute <= 59) {

          Serial.printf("⚡ Опережение: микрик сработал на минуте %02d\n",
                        microTriggerMinute);
          microTriggeredDuringMove = false;
          SET_STATE(CORRECT_ADVANCE, now);
          return;
        }

        // --- ИДЕАЛЬНЫЙ НОЛЬ ---
        if (zeroTransitionActive && microTriggeredDuringMove && microTriggerMinute == 0) {

          Serial.println("Идеальный ноль → точная доводка");
          SET_STATE(CORRECT_FINE, now);
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

        // --- НОРМАЛЬНЫЙ НОЛЬ после Нормальный ноль после - сработки не ожидается ---
        if (zeroTransitionActive && !microTriggeredDuringMove && correctionThisHour) {

          Serial.println("Нормальный ноль после корекции опережения");
          SET_STATE(IDLE, now);
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

        // --- 0) Первый вход в состояние ---
        if (!started) {
          // 0a) Микрик мог сработать ПРЯМО после MOVING
          if (microSw()) {
            Serial.println("Микрик сработал сразу после MOVING — это НЕ лаг, выполняем точную доводку");
            microTriggeredDuringMove = false;
            correctionThisHour = false;
            SET_STATE(CORRECT_FINE, now);
            return;
          }
          // 0b) Запускаем коррекцию
          Serial.println("Коррекция отставания: начинаем коррекцию");

          targetSteps = StepsForMinute * 15;
          microTriggeredDuringMove = false;

          stepper.move(targetSteps);
          started = true;
          return;
        }

        if (stepper.isRunning()) {  // --- 1) Ловим микрик во время движения ---

          if (microSw()) {
            Serial.println("Коррекция отставания: микрик сработал — здесь СТОП!");
            float movedSteps = stepper.currentPosition();
            correctionSteps = movedSteps;  // Запоминаем к-во шагов корректировки для json
            lastCorrectionMinutes = movedSteps / (float)StepsForMinute;

            stepper.stop();
            while (stepper.isRunning()) stepper.run();

            stepper.disableOutputs();
            delay(5);
            stepper.enableOutputs();

            stepper.setCurrentPosition(0);
            started = false;
            microTriggeredDuringMove = false;
            correctionThisHour = false;

            SET_STATE(CORRECT_FINE, now);
            return;
          }

          return;
        }

        // --- 2) Движение завершилось, но микрик НЕ сработал ---
        Serial.println("❌ ERROR: Lag correction exceeded 15 minutes!");
        started = false;
        microTriggeredDuringMove = false;
        SET_STATE(IDLE, now);
        return;
      }

    case CORRECT_ADVANCE:  // ----------- Коррекция опережения -----------
      {
        static bool started = false;
        static long targetSteps = 0;
        static int advanceMinutes = 0;  // сохранени минут корректировки

        if (!started) {

          int advanceMinutes = 60 - microTriggerMinute;
          if (advanceMinutes <= 0 || advanceMinutes > 15)
            advanceMinutes = 1;

          targetSteps =
            -(advanceMinutes * StepsForMinute) + (long)(StepsForMinute * correctionPercent);

          Serial.printf("Коррекция опережения: откат назад %d мин + доводка %.0f%% = %ld шагов\n",
                        advanceMinutes, correctionPercent * 100, targetSteps);

          stepper.move(targetSteps);
          started = true;
          return;
        }

        if (stepper.isRunning())
          return;
        stepper.setCurrentPosition(0);
        correctionThisHour = true;
        microTriggerMinute = -1;
        started = false;

        correctionSteps = targetSteps;                                  // Шаги для отправки в json
        lastCorrectionMinutes = -(advanceMinutes - correctionPercent);  // Сохраняем посл.коррекцию для json (со знаком минус)
        Serial.println("Коррекция опережения завершена");
        SET_STATE(IDLE, now);
        return;
      }

    case CORRECT_FINE:  // ----------- Точная доводка до нуля -----------
      {
        long fineSteps = (long)(StepsForMinute * correctionPercent);
        correctionSteps = fineSteps;                // для отправки в json
        lastCorrectionMinutes = correctionPercent;  // для отправки в json
        Serial.printf("CORRECT_FINE: доводка %ld шагов (%.0f%%)\n",
                      fineSteps, correctionPercent * 100);

        stepper.move(fineSteps);

        while (stepper.isRunning())
          stepper.run();

        stepper.setCurrentPosition(0);
        SET_STATE(IDLE, now);
        return;
      }
  }
}

// ----------------------- Микрик -----------------------
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