#include <Arduino.h>
#include "config.h"
#include "robot_motion.h"
#include "servo_manager.h"
#include "servo_motion.h"

ServoManager servoManager;
ServoMotion servoMotion(servoManager);
RobotMotion robotMotion(servoMotion);

// Direct-servo keypresses bypass RobotMotion and drive ServoMotion straight.
// Use this duration for all of them so the visual feedback is consistent.
static constexpr uint32_t kDirectMoveMs = 250;

static void printHelp() {
    Serial.println();
    Serial.println("=== Robot debug keys ===");
    Serial.println(" Direct servos (lowercase = down/back, near-shift = up/forward):");
    Serial.println("   q/a   FrontLeft  up/down");
    Serial.println("   w/s   FrontRight up/down");
    Serial.println("   e/d   RearLeft   up/down");
    Serial.println("   r/f   RearRight  up/down");
    Serial.println("   t/g   Translation forward/backward");
    Serial.println("   y/h   Rotation    left/right");
    Serial.println("   b     Body actuators -> neutral");
    Serial.println(" Movements (queued):");
    Serial.println("   i / k Step forward / backward (one full stride)");
    Serial.println("   j / l Rotate left / right (~degreesPerHalfRotation x 2)");
    Serial.println("   c     Crouch");
    Serial.println("   v     Stand (full neutral pose)");
    Serial.println("   ?     This help");
    Serial.println(" Servo pin mapping:");
    Serial.printf("   FrontLeft   = GPIO %d\n", PIN_SERVO_1);
    Serial.printf("   FrontRight  = GPIO %d\n", PIN_SERVO_2);
    Serial.printf("   RearLeft    = GPIO %d\n", PIN_SERVO_3);
    Serial.printf("   RearRight   = GPIO %d\n", PIN_SERVO_4);
    Serial.printf("   Rotation    = GPIO %d\n", PIN_SERVO_5);
    Serial.printf("   Translation = GPIO %d\n", PIN_SERVO_6);
    Serial.println();
}

static void handleKey(char c) {
    const auto& cfg = robotMotion.config;
    switch (c) {
        // --- Direct leg control ---
        case 'q':
            servoMotion.moveTo(ServoId::FrontLeft, cfg.legUpFL, kDirectMoveMs);
            break;
        case 'a':
            servoMotion.moveTo(ServoId::FrontLeft, cfg.legDownFL, kDirectMoveMs);
            break;
        case 'w':
            servoMotion.moveTo(ServoId::FrontRight, cfg.legUpFR, kDirectMoveMs);
            break;
        case 's':
            servoMotion.moveTo(ServoId::FrontRight, cfg.legDownFR, kDirectMoveMs);
            break;
        case 'e':
            servoMotion.moveTo(ServoId::RearLeft, cfg.legUpRL, kDirectMoveMs);
            break;
        case 'd':
            servoMotion.moveTo(ServoId::RearLeft, cfg.legDownRL, kDirectMoveMs);
            break;
        case 'r':
            servoMotion.moveTo(ServoId::RearRight, cfg.legUpRR, kDirectMoveMs);
            break;
        case 'f':
            servoMotion.moveTo(ServoId::RearRight, cfg.legDownRR, kDirectMoveMs);
            break;

        // --- Direct body-actuator control ---
        case 't':
            servoMotion.moveTo(ServoId::Translation, cfg.translationForward, kDirectMoveMs);
            break;
        case 'g':
            servoMotion.moveTo(ServoId::Translation, cfg.translationBackward, kDirectMoveMs);
            break;
        case 'y':
            servoMotion.moveTo(ServoId::Rotation, cfg.rotationLeft, kDirectMoveMs);
            break;
        case 'h':
            servoMotion.moveTo(ServoId::Rotation, cfg.rotationRight, kDirectMoveMs);
            break;
        case 'b':
            servoMotion.moveTo(ServoId::Translation, cfg.translationNeutral, kDirectMoveMs);
            servoMotion.moveTo(ServoId::Rotation, cfg.rotationNeutral, kDirectMoveMs);
            break;

        // --- Queued whole-robot movements ---
        case 'i':
            robotMotion.step(1);
            break;
        case 'k':
            robotMotion.step(-1);
            break;
        case 'j':
            robotMotion.rotate(cfg.degreesPerHalfRotation * 2.0f);
            break;
        case 'l':
            robotMotion.rotate(-cfg.degreesPerHalfRotation * 2.0f);
            break;
        case 'c':
            robotMotion.crouch();
            break;
        case 'v':
            robotMotion.stand();
            break;

        case '?':
            printHelp();
            break;

        case '\r':
        case '\n':
        case ' ':
            break;  // ignore line endings and spaces
        default:
            Serial.printf("Unknown key '%c' (0x%02x). Press '?' for help.\n", c, (unsigned)c);
            break;
    }
}

static void processSerialInput() {
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;
        handleKey((char)c);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Starting...");
    Serial.printf("Free heap: %u, PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

    servoManager.addServo(ServoId::FrontLeft, PIN_SERVO_1, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::FrontRight, PIN_SERVO_2, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::RearLeft, PIN_SERVO_3, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::RearRight, PIN_SERVO_4, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 90, 180, 90);
    servoManager.addServo(ServoId::Rotation, PIN_SERVO_5, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.addServo(ServoId::Translation, PIN_SERVO_6, SERVO_MIN_PULSE, SERVO_MAX_PULSE, 0, 180, 90);
    servoManager.begin();
    robotMotion.begin();

    printHelp();
}

void loop() {
    processSerialInput();
    servoMotion.update();
    robotMotion.update();
}
