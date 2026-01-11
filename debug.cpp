// debug.cpp Всё, что связано с логами, измерением параметров и прочее
#include <Arduino.h>
#include "main.h"
#include <WiFi.h>
#include <WebServer.h>
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

// ====== Лог буфер ======
static String lastLogLine;

static void logStore(const String& line) {
  lastLogLine = line;
}

String debugGetLog() {
  String out = lastLogLine;
  lastLogLine = "";  // сразу очищаем, чтобы не прислать повторно
  return out;
}

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
  logStore(line);
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

// ====== WebServer ======
static WebServer dbgServer(80);

void webMonitorBegin() {
  // Главная страница
  dbgServer.on("/", HTTP_GET, []() {
    String html = F(
      "<!doctype html><html><meta charset='utf-8'><title>XIAO ESP32 Monitor</title>"
      "<style>body{font-family:sans-serif;margin:20px}pre{background:#111;color:#0f0;padding:8px;height:450px;width:550px;overflow:auto;white-space:pre-wrap}</style>"
      "<h2>Ancient Clock Web Monitor</h2>"
      "<div id='status'></div>"
      "<h3>Log</h3><pre id='log'></pre>"
      "<script>"
      "async function refresh(){"
      "let r=await fetch('/status');"
      "let j=await r.json();"
      "document.getElementById('status').innerHTML="
      "\"<b>RTC:</b> \"+j.rtc+\"<br>\" +"
      "\"<b>Uptime:</b> \"+j.uptime+\"<br>\" +"
      "\"<b>Steps For Minute:</b> \"+j.steps+\"<br>\" +"
      "\"<b>Tail Angle:</b> \"+j.tailAngle+\"<br>\" +"
      "\"<b>EndStop:</b> \"+j.switch+\"<br>\" +"
      "\"<b>Transition Time:</b> \"+j.transition+\" sec<br>\" +"
      "\"<b>FSM:</b> \"+j.state+\"<br>\";"
      "let logElem=document.getElementById('log');"
      "let newLog=await (await fetch('/log')).text();"
      "if (!logElem.textContent.endsWith(newLog)) {logElem.textContent += newLog;}"
      "logElem.scrollTop=logElem.scrollHeight;"
      "}"
      "refresh();setInterval(refresh,2000);"
      "</script></html>");

    dbgServer.send(200, "text/html; charset=utf-8", html);
  });

  // JSON статус
  dbgServer.on("/status", HTTP_GET, []() {
    char buf[32];
    DateTime now = getCurrentTime();
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());

    String json = "{";
    json += "\"rtc\":\"" + String(buf) + "\",";
    json += "\"uptime\":\"" + uptimeStr() + "\",";
    json += "\"steps\":" + String(StepsForMinute) + ",";
    json += "\"tailAngle\":" + String(tailAngle) + ",";
    json += "\"switch\":\"" + String(microSwRaw() ? "HIGH" : "LOW") + "\",";
    json += "\"transition\":" + String(transitionTimeSec) + ",";
    json += "\"state\":\"" + String(stateName(arrowState)) + "\"";
    json += "}";

    dbgServer.send(200, "application/json; charset=utf-8", json);
  });

  // Лог
  dbgServer.on("/log", HTTP_GET, []() {
    dbgServer.send(200, "text/plain; charset=utf-8", debugGetLog());
  });

  dbgServer.begin();
}

void webMonitorLoop() {
  dbgServer.handleClient();
}

// ====== Функция возвращает параметны приложения по команде 'd' в serial monitor ======
void debugDump(DateTime now, bool microSwitchTriggered) {
  String line;
  line = String("🕰 RTC: ") + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  Serial.println(line);
  logStore(line);

  line = String("⏱ lastRtcMinute: ") + lastRtcMinute;  // Добавлено 27 сентября
  Serial.println(line);
  logStore(line);

  line = String("🎯 Current Position: ") + stepper.currentPosition();
  Serial.println(line);
  logStore(line);

  line = String("📍 stepperMaxSpeed: ") + stepperMaxSpeed;
  Serial.println(line);
  logStore(line);

  line = String("📐 stepperAcceleration: ") + stepperAcceleration;
  Serial.println(line);
  logStore(line);

  line = String("🦶 StepsForMinute: ") + StepsForMinute;
  Serial.println(line);
  logStore(line);
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
