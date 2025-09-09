void debugDump(DateTime now, bool microSwitchState) {
  Serial.println("📊 Состояние FSM и переменных:");
  Serial.printf("🕰 RTC: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
  Serial.printf("🎯 arrowState: %d\n", arrowState);
  Serial.printf("📍 stepper.currentPosition(): %d\n", stepper.currentPosition());
  Serial.printf("📐 stepper.distanceToGo(): %d\n", stepper.distanceToGo());
  Serial.printf("🔘 microSwitchState: %s\n", microSwitchState ? "ON" : "OFF");
  Serial.printf("🦶 StepsForMinute: %d\n", StepsForMinute);
}
