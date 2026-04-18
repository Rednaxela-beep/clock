// debug.cpp Всё, что связано с логами, измерением параметров и прочее
#include <Arduino.h>
#include "main.h"
#include <WiFi.h>
#include "debug.h"
#include "main.h"
#include "config.h"
#include "arrow.h"
#include "chimes.h"  // для управления молоточком
#include "wi-fi.h"

extern ArrowState arrowState;
extern int StepsForMinute;
extern int tailAngle;
// extern bool microSwitchTriggered;

void debugLogf(const char* fmt, ...) {
  char msgBuf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
  va_end(args);

  DateTime now = getCurrentTime();
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d;",
           now.hour(), now.minute(), now.second());

  String line = String(timeBuf) + String(msgBuf) + "\n";
  Serial.println(line);
}

// ====== Uptime ======
static String uptimeStr() {
  unsigned long ms = millis();
  unsigned long min = ms / 60000;  // сразу минуты
  unsigned long hr = min / 60;
  unsigned long day = hr / 24;
  char buf[32];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu",
           day, hr % 24, min % 60);
  return String(buf);
}

// ====== Функция возвращает параметны приложения по команде 'd' в serial monitor ======
void debugDump(DateTime now, bool microSwitchTriggered) {
  String line;
  line = String("🕰 RTC: ") + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  Serial.println(line);

  line = String("⏱ lastRtcMinute: ") + lastRtcMinute;  // Добавлено 27 сентября
  Serial.println(line);

  line = String("🎯 Current Position: ") + stepper.currentPosition();
  Serial.println(line);

  line = String("📍 stepperMaxSpeed: ") + stepperMaxSpeed;
  Serial.println(line);

  line = String("📐 stepperAcceleration: ") + stepperAcceleration;
  Serial.println(line);

  line = String("🦶 StepsForMinute: ") + StepsForMinute;
  Serial.println(line);
}

// ====== Единая точка обработки команд из Serial ======
void debugSerialLoop() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "d") {
    DateTime now = getCurrentTime();
    debugDump(now, microSwitchTriggered);
    return;
  }

  if (cmd.startsWith("hit")) {
    int count = 1;
    if (cmd.length() > 4) {
      count = constrain(cmd.substring(4).toInt(), 1, 12);
    }
    Serial.printf("🔨 Ударов запрошено: %d\n", count);
    hit(count);

  } else if (cmd.startsWith("lift ")) {
    int val = cmd.substring(5).toInt();
    if (val >= 0 && val <= 180) {
      liftAngle = val;
      Serial.printf("🎚️ Новый liftAngle: %d°\n", liftAngle);
    } else {
      Serial.println("❌ Угол должен быть от 0 до 180");
    }

  } else if (cmd.startsWith("tail ")) {
    int val = cmd.substring(5).toInt();
    if (val >= 0 && val <= 180) {
      tailAngle = val;
      Serial.printf("🎯 Новый tailAngle: %d°\n", tailAngle);
    } else {
      Serial.println("❌ Угол должен быть от 0 до 180");
    }

  } else if (cmd.startsWith("speed ")) {
    int val = cmd.substring(6).toInt();
    if (val >= 1 && val <= 100) {
      liftSpeed = val;
      Serial.printf("⏱️ Новая скорость взвода: %d мс/шаг\n", liftSpeed);
    } else {
      Serial.println("❌ Скорость должна быть от 1 до 100 мс");
    }

  } else if (cmd.startsWith("pause ")) {
    int val = cmd.substring(6).toInt();
    if (val >= 0 && val <= 5000) {
      pauseBetweenHits = val;
      Serial.printf("⏸️ Пауза между ударами: %d мс\n", pauseBetweenHits);
    } else {
      Serial.println("❌ Пауза должна быть от 0 до 5000 мс");
    }

  } else if (cmd == "status") {
    Serial.println("📊 Текущие параметры молоточка:");
    Serial.printf("  liftAngle = %d°\n", liftAngle);
    Serial.printf("  tailAngle = %d°\n", tailAngle);
    Serial.printf("  liftSpeed = %d мс/шаг\n", liftSpeed);
    Serial.printf("  pauseBetweenHits = %d мс\n", pauseBetweenHits);
  } else {
    Serial.println("❓ Неизвестная команда. Примеры:");
    Serial.println("  hit");
    Serial.println("  lift 120");
    Serial.println("  tail 180");
    Serial.println("  speed 15");
    Serial.println("  pause 500");
    Serial.println("  status");
    Serial.println("  d");
  }
}
