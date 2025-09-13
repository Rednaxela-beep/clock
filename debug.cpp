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

  DateTime now = rtc.now();
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d;",
           now.hour(), now.minute(), now.second());

String line = String(timeBuf) + String(msgBuf);
// + "\n";
Serial.print(line);
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
      "let logElem=document.getElementById('log');"
      "let newLog=await (await fetch('/log')).text();"
      "if (!logElem.textContent.endsWith(newLog)) {logElem.textContent += newLog;}"
      "logElem.scrollTop=logElem.scrollHeight;"
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
