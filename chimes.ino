// v.3 Танцы с молоточком ;-) Hижнее положение молоточка
int liftAngle = 110;  // угол удержания молоточка (меньше угол - выше молоточек)
int tailAngle = 126;  // отпускание (удар) и если ставим 161-162 получается приглушенный ночной удар
int liftSpeed = 50;  // (1-100) задержка шагов в мс при взводе чтобы меньше шума ;-)
int hitPrepDelay = 500;       // Пауза между взводом и ударом
int pauseBetweenHits = 10;  // пауза перед следующим ударом
const int SERVO_PIN = D6;

void chimesetup() {  
  delay(100);
  Serial.println("Инициализация через крайнее положение...");
  sg90.attach(SERVO_PIN, 500, 2400); // подключаем серво
  sg90.write(180);                   // тянем в край - "нулевой" упор
  delay(1000);                        // время на достижение точки
  sg90.write(tailAngle);             // возвращаемся в позицию ожидания
  delay(500);                        // стабилизация
  sg90.detach();                     // отцепляем, чтобы не дрожало
  }


void smoothLift(int fromAngle, int toAngle) { // ?? Вспомогательная функция плавного взвода
  if (fromAngle == toAngle) return;
  int direction = (fromAngle < toAngle) ? 1 : -1;
  for (int a = fromAngle; a != toAngle; a += direction) {
    sg90.write(a);
    delay(liftSpeed); // используем переменную скорости
    }
    sg90.write(toAngle); // финальная точка
}

void hit(int count) {
  count = constrain(count, 1, 12);
  Serial.print("⚡ Запускаем бой: ");
  Serial.print(count);
  Serial.println(" ударов");

  sg90.attach(SERVO_PIN, 500, 2400);
  sg90.setPeriodHertz(50);  // стабильный частотный режим

  for (int i = 0; i < count; i++) {
    Serial.print("🔼 Взвод №"); Serial.println(i + 1);

    smoothLift(tailAngle, liftAngle);   // поднятие молоточка
    delay(hitPrepDelay);                // выдержка перед ударом

    sg90.write(tailAngle);              // УДАР!
    Serial.println("⚡ Удар!");
    delay(100);                         // стабилизация

    if (i < count - 1) delay(pauseBetweenHits);
  }

  sg90.detach();  // убираем шим, если не нужен до следующего часа
  Serial.println("✅ Бой завершён.");
}

void chimesloop() {  
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("hit")) {
      int count = 1;
      if (cmd.length() > 4) {
        count = cmd.substring(4).toInt();
        count = constrain(count, 1, 12); // ограничим диапазон
      }

      Serial.print("Ударов запрошено: ");
      Serial.println(count);

      for (int i = 0; i < count; i++) {
        sg90.attach(SERVO_PIN, 500, 2400);

        Serial.print("Взвод №");
        Serial.println(i + 1);
        int currentAngle = tailAngle; // предположим текущую позицию
        smoothLift(currentAngle, liftAngle); // плавный взвод
        delay(hitPrepDelay);    // дать серво время на взвод...
        sg90.attach(SERVO_PIN, 500, 2400);
        delay(100); // дать SG90 время на инициализацию
        sg90.write(tailAngle);  // и удар! 

        Serial.println("Удар!");
        delay(100); // стабилизация
        sg90.detach();

        if (i < count - 1) {
          delay(pauseBetweenHits); // пауза между ударами
        }
      }
      Serial.println("Серия завершена.");

    } else if (cmd.startsWith("lift ")) {
      int val = cmd.substring(5).toInt();
      if (val >= 0 && val <= 180) {
        liftAngle = val;
        Serial.print("Новый liftAngle: ");
        Serial.println(liftAngle);
      } else {
        Serial.println("Угол должен быть от 0 до 180");
      }

    } else if (cmd.startsWith("tail ")) {
      int val = cmd.substring(5).toInt();
      if (val >= 0 && val <= 180) {
        tailAngle = val;
        Serial.print("? Новый tailAngle: ");
        Serial.println(tailAngle);
      } else {
        Serial.println("Угол должен быть от 0 до 180");
      }

    } else if (cmd.startsWith("speed ")) {
      int val = cmd.substring(6).toInt();
      if (val >= 1 && val <= 100) {
        liftSpeed = val;
        Serial.print("Новая скорость взвода: ");
        Serial.print(liftSpeed);
        Serial.println(" мс/шаг");
      } else {
        Serial.println("Скорость должна быть от 1 до 100 мс");
      }

    } else if (cmd == "status") {
      Serial.println("Текущие параметры:");
      Serial.print("liftAngle = ");
      Serial.print(liftAngle);
      Serial.println("°");
      Serial.print("tailAngle = ");
      Serial.print(tailAngle);
      Serial.println("°");
      Serial.print("liftSpeed = ");
      Serial.print(liftSpeed);
      Serial.println(" мс/шаг");

    } else {
      Serial.println("Неизвестная команда. Примеры:");
      Serial.println("  hit");
      Serial.println("  lift 120");
      Serial.println("  tail 180");
      Serial.println("  speed 15");
      Serial.println("  status");
    }
  }
}
