#include <Arduino.h>
#include "config.h"
#include "servo_manager.h"

ServoManager servoManager(6);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Starting...");
    Serial.printf("Free heap: %u, PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

    servoManager.addServo(PIN_SERVO_1, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(PIN_SERVO_2, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(PIN_SERVO_3, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(PIN_SERVO_4, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(PIN_SERVO_5, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(PIN_SERVO_6, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.begin();
}

void loop() {
}
