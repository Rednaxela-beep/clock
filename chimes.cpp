// chimes.cpp -- Функции управления боя
#include <Arduino.h>
#include "chimes.h"
#include "config.h"  // для SERVO_PIN и диапазонов

// Глобальные параметры боя (начальные значения)
// ⚙️ Параметры удара
int liftAngle = 0;           // Угол взвода (молоточек вверху)
int tailAngle = 18;          // Угол удара (молоточек отпущен)
int liftSpeed = 30;          // Скорость взвода (мс/шаг)
int tailSpeed = 0;           // Скорость начального взвода при запуске
int pauseBetweenHits = 500;  // Пауза между ударами (мс)

void smoothMove(int fromAngle, int toAngle, int speed) {
  if (fromAngle == toAngle) return;
  int direction = (fromAngle < toAngle) ? 1 : -1;
  for (int a = fromAngle; a != toAngle; a += direction) {
    sg90.write(a);
    delay(speed);
  }
  sg90.write(toAngle);
}

void chimesetup() {
  delay(100);
  Serial.println("🔧 ClockHammer v3 — инициализация...");

  sg90.attach(SERVO_PIN, 500, 2400);
  Serial.println("🔼 Установка нулевой позиции...");
  smoothMove(90, 0, liftSpeed);  // быстрый уход в 0
  delay(300);
  smoothMove(0, liftAngle, tailSpeed);  // плавный взвод
  delay(300);                           // стабилизация
  sg90.detach();                        // отцепляем, чтобы не дрожало

  Serial.println("✅ Молоточек взведён и готов к удару.");
  Serial.println("🛠️ Команды: hit <число>, chimes"); // По команде chimes выдать значения параметров удара
}

void hit(int count) {
  count = constrain(count, 1, 12);
  Serial.printf("⚡ Запускаем бой: %d ударов\n", count);

  sg90.attach(SERVO_PIN, 500, 2400);
  sg90.setPeriodHertz(50);  // стабильный частотный режим

  for (int i = 0; i < count; i++) {
    Serial.printf("🔼 Взвод №%d\n", i + 1);

    smoothLift(tailAngle, liftAngle);  // поднятие молоточка
    delay(hitPrepDelay);               // выдержка перед ударом

    sg90.write(tailAngle);  // УДАР!
    Serial.println("⚡ Удар!");
    delay(100);  // стабилизация

    if (i < count - 1) delay(pauseBetweenHits);
  }

  sg90.detach();
  Serial.println("✅ Бой завершён.");
}

void chimesloop() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("hit")) {
    int count = 1;
    if (cmd.length() > 4) {
      count = constrain(cmd.substring(4).toInt(), 1, 12);
    }
    Serial.printf("Ударов запрошено: %d\n", count);
    hit(count);

  } else if (cmd.startsWith("lift ")) {
    int val = cmd.substring(5).toInt();
    if (val >= 0 && val <= 180) {
      liftAngle = val;
      Serial.printf("Новый liftAngle: %d\n", liftAngle);
    } else {
      Serial.println("Угол должен быть от 0 до 180");
    }

  } else if (cmd.startsWith("tail ")) {
    int val = cmd.substring(5).toInt();
    if (val >= 0 && val <= 180) {
      tailAngle = val;
      Serial.printf("Новый tailAngle: %d\n", tailAngle);
    } else {
      Serial.println("Угол должен быть от 0 до 180");
    }

  } else if (cmd.startsWith("speed ")) {
    int val = cmd.substring(6).toInt();
    if (val >= 1 && val <= 100) {
      liftSpeed = val;
      Serial.printf("Новая скорость взвода: %d мс/шаг\n", liftSpeed);
    } else {
      Serial.println("Скорость должна быть от 1 до 100 мс");
    }

  } else if (cmd == "status") {
    Serial.println("Текущие параметры:");
    Serial.printf("liftAngle = %d°\n", liftAngle);
    Serial.printf("tailAngle = %d°\n", tailAngle);
    Serial.printf("liftSpeed = %d мс/шаг\n", liftSpeed);

  } else {
    Serial.println("Неизвестная команда. Примеры:");
    Serial.println("  hit");
    Serial.println("  lift 120");
    Serial.println("  tail 180");
    Serial.println("  speed 15");
    Serial.println("  status");
  }
}
