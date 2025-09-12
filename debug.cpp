// debug.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "debug.h"
#include "main.h"
#include "config.h"
#include "version.h"
#include "arrow.h"

extern ArrowState arrowState;
extern int StepsForMinute;

// ====== Лог буфер ======
static String logBuffer[50];
static int logIndex = 0;

static void logStore(const String& line) {
  logBuffer[logIndex] = line;
  logIndex = (logIndex + 1) % 50;
}

String debugGetLog() {
  String out;
  int idx = logIndex;
  for (int i = 0; i < 50; i++) {
    const String& line = logBuffer[idx];
    if (line.length()) out += line + "\n";
    idx = (idx + 1) % 50;
  }
  return out;
}

void debugLogf(const char *fmt, ...) { // "Обёрточный логгер" - единая точка вывода логов для замены Serial.println
    char buf[128]; // подбери размер под свои сообщения
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(buf);
    logStore(String(buf));
}

// ====== Uptime ======
static String uptimeStr() {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;
  unsigned long hr = min / 60;
  unsigned long day = hr / 24;
  char buf[32];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
           day, hr % 24, min % 60, sec % 60);
  return String(buf);
}

// ====== WebServer ======
static WebServer dbgServer(80);

void webMonitorBegin() {
  // Главная страница
  dbgServer.on("/", HTTP_GET, []() {
    String html = F(
      "<!doctype html><html><meta charset='utf-8'><title>XIAO ESP32 Monitor</title>"
      "<style>body{font-family:sans-serif;margin:20px}pre{background:#111;color:#0f0;padding:8px;height:400px;width:500px;overflow:auto;white-space:pre-wrap}</style>"
      "<h2>Ancient Clock Web Monitor</h2>"
      "<div id='status'></div>"
      "<form onsubmit='return setSteps()'>"
      "StepsForMinute: <input id='steps' type='number'><input type='submit' value='Set'>"
      "</form>"
      "<h3>Log</h3><pre id='log'></pre>"
      "<script>"
      "async function refresh(){"
      "let r=await fetch('/status');"
      "let j=await r.json();"
      "document.getElementById('status').innerHTML="
      "'<b>RTC:</b> '+j.rtc+'<br>' +"
      "'<b>Uptime:</b> '+j.uptime+'<br>' +"
      "'<b>StepsForMinute:</b> '+j.steps+'<br>' +"
      "'<b>FSM:</b> '+j.state;"
      "document.getElementById('steps').value=j.steps;"
      "let log=await fetch('/log');"
      "document.getElementById('log').textContent=await log.text();"
      "}"
      "async function setSteps(){"
      "let val=document.getElementById('steps').value;"
      "await fetch('/setSteps?val='+val,{method:'POST'});"
      "refresh();return false;"
      "}"
      "refresh();setInterval(refresh,2000);"
      "</script></html>");
    dbgServer.send(200, "text/html; charset=utf-8", html);
  });

// JSON статус
  dbgServer.on("/status", HTTP_GET, []() {
    char buf[32];
    DateTime now = rtc.now();
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             now.hour(), now.minute(), now.second());
    String json = "{";
    json += "\"rtc\":\"" + String(buf) + "\",";
    json += "\"uptime\":\"" + uptimeStr() + "\",";
    json += "\"steps\":" + String(StepsForMinute) + ",";
    json += "\"state\":\"" + String(stateName(arrowState)) + "\"";
    json += "}";
    dbgServer.send(200, "application/json; charset=utf-8", json);
  });

// Лог
  dbgServer.on("/log", HTTP_GET, []() {
    dbgServer.send(200, "text/plain; charset=utf-8", debugGetLog());
  });

  // Установка StepsForMinute
  dbgServer.on("/setSteps", HTTP_POST, []() {
    if (dbgServer.hasArg("val")) {
      StepsForMinute = dbgServer.arg("val").toInt();
      dbgServer.send(200, "text/plain", "OK");
    } else {
      dbgServer.send(400, "text/plain", "Missing val");
    }
  });

  dbgServer.begin();
}

void webMonitorLoop() {
  dbgServer.handleClient();
}

// ====== Функция возвращает параметны приложения ======
void debugDump(DateTime now, bool microSwitchState) {
  String line;
  line = String("🕰 RTC: ") + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  Serial.println(line);
  logStore(line);

  line = String("🎯 arrowState: ") + stateName(arrowState);
  Serial.println(line);
  logStore(line);

  line = String("📍 stepper.currentPosition(): ") + stepper.currentPosition();
  Serial.println(line);
  logStore(line);

  line = String("📐 stepper.distanceToGo(): ") + stepper.distanceToGo();
  Serial.println(line);
  logStore(line);

  line = String("🔘 microSwitchState: ") + (microSwitchState ? "ON" : "OFF");
  Serial.println(line);
  logStore(line);

  line = String("🦶 StepsForMinute: ") + StepsForMinute;
  Serial.println(line);
  logStore(line);
}
