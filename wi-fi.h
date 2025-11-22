//wi-fi.h Объвление нужных для сети переменных 
#pragma once
#include <RTClib.h>
void connectToWiFi();
// Синхронизация RTC по NTP
DateTime syncRTC();

// Ежечасная синхронизация (вызов из loopMain)
void handleHourlySync(DateTime now);
