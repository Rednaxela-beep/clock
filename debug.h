#pragma once
#include <Arduino.h>
#include <RTClib.h>

void debugDump(DateTime now);

#define DEBUG_LEVEL 2  // 0 = off, 1 = basic, 2 = full

#if DEBUG_LEVEL > 0
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTF(...)
#endif
