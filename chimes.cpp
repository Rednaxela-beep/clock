#include <Arduino.h>
#include "chimes.h"
#include "config.h"  // для SERVO_PIN и диапазонов

// Глобальные параметры боя (начальные значения)
int liftAngle         = 110;  // меньше угол — выше молоточек
int tailAngle         = 126;  // 161–162 = приглушённый ночной удар
int liftSpeed         = 50;   // (1–100) задержка шагов в мс при взводе
int hitPrepDelay      = 500;  // пауза между взводом и ударом
int pauseBetweenHits  = 10;   // пауза перед следующим ударом

void chimesetup() {  
    delay(100);
    Serial.println("Инициализация через крайнее положение...");
    sg90.attach(SERVO_PIN, 500, 2400); // подключаем серво
    sg90.write(180);                   // тянем в край — "нулевой" упор
    delay(1000);                       // время на достижение точки
    sg90.write(tailAngle);             // возвращаемся в позицию ожидания
    delay(500);                        // стабилизация
    sg90.detach();                     // отцепляем, чтобы не дрожало
}

void smoothLift(int fromAngle, int toAngle) {
    if (fromAngle == toAngle) return;
    int direction = (fromAngle < toAngle) ? 1 : -1;
    for (int a = fromAngle; a != toAngle; a += direction) {
        sg90.write(a);
        delay(liftSpeed);
    }
    sg90.write(toAngle); // финальная точка
}

void hit(int count) {
    count = constrain(count, 1, 12);
    Serial.printf("⚡ Запускаем бой: %d ударов\n", count);

    sg90.attach(SERVO_PIN, 500, 2400);
    sg90.setPeriodHertz(50);  // стабильный частотный режим

    for (int i = 0; i < count; i++) {
        Serial.printf("🔼 Взвод №%d\n", i + 1);

        smoothLift(tailAngle, liftAngle); // поднятие молоточка
        delay(hitPrepDelay);              // выдержка перед ударом

        sg90.write(tailAngle);            // УДАР!
        Serial.println("⚡ Удар!");
        delay(100);                       // стабилизация

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
