// chimes.cpp -- Функции управления боя
#include <Arduino.h>
#include "main.h"  // доступ к главному модулю
// #include "debug.h"
#include "chimes.h"
#include "config.h"  // для SERVO_PIN и диапазонов

void smoothMove(int fromAngle, int toAngle, int speed) {
  if (fromAngle == toAngle) return;
  int direction = (fromAngle < toAngle) ? 1 : -1;
  for (int a = fromAngle; a != toAngle; a += direction) {
    mg90.write(a);
    delay(speed);
  }
  mg90.write(toAngle);
}

void chimesetup() {
  mg90.detach();
  delay(100);
  Serial.println("🔧 ClockHammer v3 — инициализация...");

  mg90.attach(SERVO_PIN, 600, 2300);  // для MG90S 600–2300
  Serial.println("🔼 Установка нулевой позиции...");
  smoothMove(90, 0, liftSpeed);  // быстрый уход в 0
  delay(300);
  smoothMove(0, liftAngle, liftSpeed);  // плавный взвод
  delay(300);
  mg90.detach();


  Serial.println("✅ Молоточек взведён и готов к удару.");
  Serial.println("🛠️ Команды: hit <число>, lift <угол>, tail <угол>, pause <мс>, status");
}

void hit(int count) {
  count = constrain(count, 1, 12);

  debugLogf("⚡Запускаем бой: %d ударов", count);
  // Serial.printf("⚡ Запускаем бой: %d ударов\n", count);

  for (int i = 0; i < count; i++) {
    Serial.printf("💥 Удар №%d ", i + 1);

    mg90.attach(SERVO_PIN, 500, 2400);
    mg90.setPeriodHertz(50);  // стабильный частотный режим

    mg90.write(tailAngle);  // УДАР!
    delay(100);             // стабилизация

    smoothMove(tailAngle, liftAngle, liftSpeed);  // взвод
    delay(100);                                   // можно убрать или оставить для плавности

    mg90.detach();

    if (i < count - 1) delay(pauseBetweenHits);
  }
  Serial.println("✅ Бой завершён.");
}
