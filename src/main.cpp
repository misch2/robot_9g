#include <Arduino.h>
#include "config.h"
#include "servo_manager.h"
#include "servo_motion.h"

ServoManager servoManager;
ServoMotion servoMotion(servoManager);

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Starting...");
    Serial.printf("Free heap: %u, PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

    servoManager.addServo(ServoId::FrontLeft, PIN_SERVO_1, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::FrontRight, PIN_SERVO_2, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::RearLeft, PIN_SERVO_3, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::RearRight, PIN_SERVO_4, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::Rotation, PIN_SERVO_5, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::Translation, PIN_SERVO_6, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.begin();
}

void loop() {
    servoMotion.update();
}
