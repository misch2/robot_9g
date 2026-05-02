#include <Arduino.h>
#include "config.h"
#include "robot_face.h"
#include "robot_motion.h"
#include "servo_manager.h"
#include "servo_motion.h"

ServoManager servoManager;
ServoMotion servoMotion(servoManager);
RobotMotion robotMotion(servoMotion);
RobotFace robotFace;

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
    Serial.println("   x     Sit (crouch + extra leg lift)");
    Serial.println("   v     Stand (full neutral pose)");
    Serial.println("   z     Dance (lift each leg clockwise)");
    Serial.println(" Display:");
    Serial.println("   m     Cycle face expression");
    Serial.println("   ?     This help");
    Serial.println(" Servo pin mapping:");
    for (const ServoSpec& spec : kServos) {
        Serial.printf("   %s = GPIO %d\n", spec.name, spec.pin);
    }
    Serial.println();
}

static void handleKey(char c) {
    const auto& cfg = robotMotion.config;
    switch (c) {
        // --- Direct leg control ---
        case 'q':
            servoMotion.moveToFraction(ServoId::FrontLeft, cfg.liftFraction, kDirectMoveMs);
            break;
        case 'a':
            servoMotion.moveToFraction(ServoId::FrontLeft, 0.0f, kDirectMoveMs);
            break;
        case 'w':
            servoMotion.moveToFraction(ServoId::FrontRight, cfg.liftFraction, kDirectMoveMs);
            break;
        case 's':
            servoMotion.moveToFraction(ServoId::FrontRight, 0.0f, kDirectMoveMs);
            break;
        case 'e':
            servoMotion.moveToFraction(ServoId::RearLeft, cfg.liftFraction, kDirectMoveMs);
            break;
        case 'd':
            servoMotion.moveToFraction(ServoId::RearLeft, 0.0f, kDirectMoveMs);
            break;
        case 'r':
            servoMotion.moveToFraction(ServoId::RearRight, cfg.liftFraction, kDirectMoveMs);
            break;
        case 'f':
            servoMotion.moveToFraction(ServoId::RearRight, 0.0f, kDirectMoveMs);
            break;

        // --- Direct body-actuator control ---
        case 't':
            servoMotion.moveToFraction(ServoId::Translation, +cfg.actuateFraction, kDirectMoveMs);
            break;
        case 'g':
            servoMotion.moveToFraction(ServoId::Translation, -cfg.actuateFraction, kDirectMoveMs);
            break;
        case 'y':
            servoMotion.moveToFraction(ServoId::Rotation, +cfg.actuateFraction, kDirectMoveMs);
            break;
        case 'h':
            servoMotion.moveToFraction(ServoId::Rotation, -cfg.actuateFraction, kDirectMoveMs);
            break;
        case 'b':
            servoMotion.moveToFraction(ServoId::Translation, 0.0f, kDirectMoveMs);
            servoMotion.moveToFraction(ServoId::Rotation, 0.0f, kDirectMoveMs);
            break;

        // --- Queued whole-robot movements ---
        case 'i':
            robotMotion.step(1);
            break;
        case 'k':
            robotMotion.step(-1);
            break;
        case 'j':
            robotMotion.rotate(-cfg.degreesPerHalfRotation * 2.0f);
            break;
        case 'l':
            robotMotion.rotate(cfg.degreesPerHalfRotation * 2.0f);
            break;
        case 'x':
            robotMotion.sit();
            break;
        case 'c':
            robotMotion.crouch();
            break;
        case 'v':
            robotMotion.stand();
            break;
        case 'z':
            robotMotion.dance();
            break;

        // --- Display ---
        case 'm':
            robotFace.cycleExpression();
            Serial.printf("Expression: %s\n",
                          RobotFace::expressionName(robotFace.getExpression()));
            break;

        case '?':
            printHelp();
            break;

        case '\r':
        case '\n':
            Serial.println();
            break;  // ignore line endings

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

    robotFace.begin();
    servoManager.begin();

    robotMotion.begin();
    // FIXME tweaking:
    robotMotion.config.liftFraction = 1.0f;


    printHelp();
}

void loop() {
    processSerialInput();
    servoMotion.update();
    robotMotion.update();

    // Show a Concentrating face while the robot is moving; restore the
    // prior expression once idle. Edge-detected so we don't fight 'm'.
    static bool wasIdle                            = true;
    static RobotFace::Expression preMoveExpression = RobotFace::Expression::Happy;
    bool idle                                      = robotMotion.isIdle();
    if (wasIdle && !idle) {
        preMoveExpression = robotFace.getExpression();
        robotFace.setExpression(RobotFace::Expression::Concentrating);
    } else if (!wasIdle && idle) {
        robotFace.setExpression(preMoveExpression);
    }
    wasIdle = idle;

    robotFace.update();
}
