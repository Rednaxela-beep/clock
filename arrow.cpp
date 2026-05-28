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
float lastCorrectionMinutes = NAN;  // Последняя коррекция. NAN = не было коррекции
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

        Serial.printf("%02d:%02d:%02d ▶️ Переход на %02d\n",
                      now.hour(), now.minute(), now.second(), targetMinute);

        zeroTransitionActive = (targetMinute == 0);
        microTriggeredDuringMove = false;

        stepper.moveTo(stepper.currentPosition() + StepsForMinute);
        moveStartPosition = stepper.currentPosition();  // запомним стартовую позицию для этого минутного хода
        stepAtTriggerPos = LONG_MIN;                    // ещё не зафиксирован
        lagStartPosition = 0;
        lagStepsToTrigger = 0;
        errorSteps = 0;
        errorMinutes = NAN;
        SET_STATE(MOVING, now);
      }
      break;

    case MOVING:  // ----------- Движение -----------
      stepper.run();

      if (microSw()) {
        microTriggeredDuringMove = true;
        microTriggerMinute = targetMinute;

        if (stepAtTriggerPos == LONG_MIN) {
          long rel = stepper.currentPosition() - moveStartPosition;
          long absSteps = abs(StepsForMinute);
          long posWithin = ((rel % absSteps) + absSteps) % absSteps;
          stepAtTriggerPos = posWithin;
        }

        // ОСТАНАВЛИВАЕМ ДВИЖЕНИЕ СРАЗУ
        stepper.stop();
        while (stepper.isRunning()) stepper.run();

        // --- ОПЕРЕЖЕНИЕ ---
        if (microTriggerMinute >= 45 && microTriggerMinute <= 59) {
          Serial.printf("⚡ Опережение: микрик сработал на минуте %02d\n", microTriggerMinute);
          SET_STATE(CORRECT_ADVANCE, now);
          return;
        }

        // --- НОРМАЛЬНЫЙ НОЛЬ ---
        if (targetMinute == 0) {
          Serial.println("Нормальный ноль → точная доводка");
          SET_STATE(CORRECT_FINE, now);
          return;
        }

        // --- Обычное срабатывание в минуте 1..44 ---
        SET_STATE(CORRECT_FINE, now);
        return;
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

          Serial.println("Нормальный ноль → точная доводка");
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

        // --- НОРМАЛЬНЫЙ НОЛЬ  после корекции опережения - сработки не ожидается ---
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
          if (microSw()) {  // 0a) Микрик мог сработать ПРЯМО после MOVING
            Serial.println("Микрик сработал сразу после MOVING — это НЕ лаг, выполняем точную доводку");
            microTriggeredDuringMove = false;
            correctionThisHour = false;
            SET_STATE(CORRECT_FINE, now);
            return;
          }
          // 0b) Запускаем коррекцию
          Serial.println("Начинаем коррекцию отставания");
          stepper.setMaxSpeed(stepperMaxSpeed / 2);  // Уменьшаем скорость вдвое, чтобы не перелетать микрик
          targetSteps = StepsForMinute * 15;
          microTriggeredDuringMove = false;

          stepper.move(targetSteps);
          started = true;
          lagStartPosition = stepper.currentPosition();
          return;
        }

        if (stepper.isRunning()) {
          if (microSw()) {
            Serial.println("Коррекция отставания: микрик сработал — СТОП!");

            // Сколько шагов прошло с начала догонки до срабатывания
            long movedSinceLagStart = stepper.currentPosition() - lagStartPosition;  // может быть положительным
            long absSteps = abs(StepsForMinute);

            // posWithinMinute — позиция срабатывания внутри виртуального минутного хода
            long posWithinMinute = ((movedSinceLagStart % absSteps) + absSteps) % absSteps;

            // Запомним для статистики
            stepAtTriggerPos = posWithinMinute;
            lagStepsToTrigger = movedSinceLagStart;

            // Вычислим истинную ошибку относительно ideal trigger point
            float triggerPointIdealAbs = absSteps * (1.0f - correctionPercent);
            errorSteps = (long)stepAtTriggerPos - (long)round(triggerPointIdealAbs);
            errorMinutes = (float)errorSteps / (float)absSteps;

            // Теперь применяем остановку и подготовку как раньше
            stepper.stop();
            while (stepper.isRunning()) stepper.run();
            stepper.disableOutputs();
            delay(5);
            stepper.enableOutputs();
            stepper.setCurrentPosition(0);
            stepper.setMaxSpeed(stepperMaxSpeed);  // ВОЗВРАЩАЕМ скорость
            started = false;
            microTriggeredDuringMove = false;
            correctionThisHour = false;

            // Для applied correction: сколько шагов мы фактически прошли в догонке
            correctionSteps = lagStepsToTrigger;
            lastCorrectionMinutes = (float)correctionSteps / (float)absSteps;

            SET_STATE(CORRECT_FINE, now);
            return;
          }
          return;
        }

        Serial.println("❌ СТОП: отставание более чем на 15 минут!");  // Движение завершилось, но микрик НЕ сработал
        stepper.setMaxSpeed(stepperMaxSpeed);                         // ВОЗВРАЩАЕМ скорость
        started = false;
        microTriggeredDuringMove = false;
        SET_STATE(IDLE, now);
        return;
      }

    case CORRECT_ADVANCE:  // ----------- Коррекция опережения -----------
      {
        static bool started = false;
        static long targetSteps = 0;
        static int advanceMinutes = 0;  // сохраняем минуты корректировки

        if (!started) {
          // Сколько минут стрелка убежала вперёд
          advanceMinutes = 60 - microTriggerMinute;
          if (advanceMinutes <= 0 || advanceMinutes > 15)
            advanceMinutes = 1;

          // Чистый откат назад на advanceMinutes минут
          targetSteps = -(advanceMinutes * StepsForMinute);

          Serial.printf("Коррекция опережения: откат назад %d мин = %ld шагов\n",
                        advanceMinutes, targetSteps);

          stepper.move(targetSteps);
          started = true;
          return;
        }

        if (stepper.isRunning())
          return;

        // Движение завершено
        stepper.setCurrentPosition(0);
        correctionThisHour = true;
        microTriggerMinute = -1;
        started = false;

        // Фиксируем реальную коррекцию опережения
        correctionSteps = targetSteps;            // шаги для json
        lastCorrectionMinutes = -advanceMinutes;  // минуты со знаком минус

        Serial.println("Коррекция опережения завершена, переходим к точной доводке");
        // После того как движение завершено и мы знаем, что было опережение
        // stepAtTriggerPos должен быть уже зафиксирован в MOVING (позиция внутри минутного хода)
        if (stepAtTriggerPos != LONG_MIN) {
          long absSteps = abs(StepsForMinute);
          float triggerPointIdealAbs = absSteps * (1.0f - correctionPercent);

          // errorSteps = фактическая позиция - идеальная позиция (в шагах)
          errorSteps = (long)stepAtTriggerPos - (long)round(triggerPointIdealAbs);
          errorMinutes = (float)errorSteps / (float)absSteps;
        } else {  // На всякий случай — если не зафиксировали (маловероятно), пометим NAN
          errorSteps = 0;
          errorMinutes = NAN;
        }

        correctionSteps = targetSteps;            // Теперь applied correction (что мы сделали) уже у тебя в correctionSteps/lastCorrectionMinutes
        lastCorrectionMinutes = -advanceMinutes;  // как у тебя было

        SET_STATE(CORRECT_FINE, now);  // всегда доводка до нуля
        return;
      }

    case CORRECT_FINE:
      {
        long fineSteps = (long)(abs(StepsForMinute) * correctionPercent);

        // Если до этого не было НИ одной грубой коррекции — фиксируем "нулевую"
        if (isnan(lastCorrectionMinutes)) {
          lastCorrectionMinutes = 0.0f;
          correctionSteps = 0;
        }

        Serial.printf("CORRECT_FINE: доводка %ld шагов (%.0f%%)\n",
                      fineSteps, correctionPercent * 100);

        // НЕ меняем correctionSteps и lastCorrectionMinutes — это не грубая коррекция
        stepper.move((StepsForMinute >= 0) ? fineSteps : -fineSteps);
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
      snprintf(buf, sizeof(buf), "Минута %02d", targetMinute);
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