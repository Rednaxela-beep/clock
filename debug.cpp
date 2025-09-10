#include <Arduino.h>
#include "debug.h"
#include "main.h"     // чтобы видеть stepper
#include "config.h"   // чтобы видеть StepsForMinute

void debugDump(DateTime now, bool microSwitchState) {
    Serial.println("📊 Состояние FSM и переменных:");
    Serial.printf("🕰 RTC: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
    Serial.printf("🎯 arrowState: %s\n", stateName(arrowState));
    Serial.printf("📍 stepper.currentPosition(): %ld\n", stepper.currentPosition());
    Serial.printf("📐 stepper.distanceToGo(): %ld\n", stepper.distanceToGo());
    Serial.printf("🔘 microSwitchState: %s\n", microSwitchState ? "ON" : "OFF");
    Serial.printf("🦶 StepsForMinute: %d\n", StepsForMinute);
}
