#include <Arduino.h>
#if defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL)
#include <Wire.h>
#endif
#include "config.h"
#include "current_sensor.h"
#include "robot_eyes.h"
#include "robot_face.h"
#include "robot_motion.h"
#include "servo_manager.h"
#include "servo_motion.h"
#include "web_control.h"

ServoManager servoManager;
ServoMotion servoMotion(servoManager);
RobotMotion robotMotion(servoMotion);
RobotFace robotFace;
RobotEyes robotEyes;
CurrentSensor currentSensor(INA219_ADDR);
WebControl webControl(servoManager, servoMotion, robotMotion, robotFace, robotEyes, currentSensor);

static constexpr uint32_t kCurrentPrintIntervalMs = 1000;

// Direct-servo keypresses bypass RobotMotion and drive ServoMotion straight.
// Use this duration for all of them so the visual feedback is consistent.
static constexpr uint32_t kDirectMoveMs = 250;

#if defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL)
static void scanI2cBus() {
    // Bring up Wire ourselves so the scan works regardless of whether the
    // servo backend has already started the bus. Wire.begin() is idempotent
    // on arduino-esp32.
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.printf("[I2C] scanning 0x03..0x77 on SDA=%d SCL=%d...\n",
                  (int)PIN_I2C_SDA, (int)PIN_I2C_SCL);
    uint8_t found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C]   device at 0x%02X\n", addr);
            found++;
        }
    }
    Serial.printf("[I2C] scan done, %u device(s) found\n", (unsigned)found);
}
#endif

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
    Serial.println("   u/o   Head        left/right");
    Serial.println("   b     Body + head actuators -> neutral");
    Serial.println(" Movements (queued):");
    Serial.println("   i / k Step forward / backward (one full stride)");
    Serial.println("   j / l Rotate left / right (~degreesPerRotation)");
    Serial.println("   c     Crouch");
    Serial.println("   x     Sit (crouch + extra leg lift)");
    Serial.println("   v     Stand (full neutral pose)");
    Serial.println("   z     Dance (lift each leg clockwise)");
    Serial.println("   n     Shake head 'no'");
    Serial.println(" Display:");
    Serial.println("   m     Cycle face expression");
    Serial.println("   9     Cycle eye1/eye2/eye3.bmp test image");
    Serial.println("   0     Identify eyes (green R / orange L)");
    Serial.println("   ?     This help");
    Serial.println(" Servo channel mapping (PCA9685):");
    for (size_t i = 0; i < sizeof(kServos) / sizeof(kServos[0]); i++) {
        Serial.printf("   %s = CH %u\n", kServos[i].name, (unsigned)i);
    }
    Serial.println();
}

static void handleKey(char c) {
    const auto& cfg = robotMotion.config;
    switch (c) {
        // --- Direct leg control ---
        case 'q':
            servoMotion.moveToFraction(ServoId::FrontLeft, cfg.liftFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'a':
            servoMotion.moveToFraction(ServoId::FrontLeft, 0.0f, cfg.scaled(kDirectMoveMs));
            break;
        case 'w':
            servoMotion.moveToFraction(ServoId::FrontRight, cfg.liftFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 's':
            servoMotion.moveToFraction(ServoId::FrontRight, 0.0f, cfg.scaled(kDirectMoveMs));
            break;
        case 'e':
            servoMotion.moveToFraction(ServoId::RearLeft, cfg.liftFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'd':
            servoMotion.moveToFraction(ServoId::RearLeft, 0.0f, cfg.scaled(kDirectMoveMs));
            break;
        case 'r':
            servoMotion.moveToFraction(ServoId::RearRight, cfg.liftFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'f':
            servoMotion.moveToFraction(ServoId::RearRight, 0.0f, cfg.scaled(kDirectMoveMs));
            break;

        // --- Direct body-actuator control ---
        case 't':
            servoMotion.moveToFraction(ServoId::Translation, +cfg.actuateFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'g':
            servoMotion.moveToFraction(ServoId::Translation, -cfg.actuateFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'y':
            servoMotion.moveToFraction(ServoId::Rotation, +cfg.actuateFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'h':
            servoMotion.moveToFraction(ServoId::Rotation, -cfg.actuateFraction, cfg.scaled(kDirectMoveMs));
            break;
        case 'u':
            servoMotion.moveToFraction(ServoId::HeadPan, +1.0f, cfg.scaled(kDirectMoveMs));
            break;
        case 'o':
            servoMotion.moveToFraction(ServoId::HeadPan, -1.0f, cfg.scaled(kDirectMoveMs));
            break;
        case 'b':
            servoMotion.moveToFraction(ServoId::Translation, 0.0f, cfg.scaled(kDirectMoveMs));
            servoMotion.moveToFraction(ServoId::Rotation, 0.0f, cfg.scaled(kDirectMoveMs));
            servoMotion.moveToFraction(ServoId::HeadPan, 0.0f, cfg.scaled(kDirectMoveMs));
            break;

        // --- Queued whole-robot movements ---
        case 'i':
            robotMotion.step(1);
            break;
        case 'k':
            robotMotion.step(-1);
            break;
        case 'j':
            robotMotion.rotate(-cfg.degreesPerRotation);
            break;
        case 'l':
            robotMotion.rotate(cfg.degreesPerRotation);
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
        case 'n':
            robotMotion.shakeNo();
            break;

        // --- Display ---
        case 'm':
            // RobotFace owns the canonical cycle logic (it knows which
            // expressions to skip); mirror the result onto RobotEyes.
            robotFace.cycleExpression();
            robotEyes.setExpression(robotFace.getExpression());
            Serial.printf("Expression: %s\n",
                          RobotFace::expressionName(robotFace.getExpression()));
            break;

        case '9': {
            int n = robotEyes.showTestImage();
            Serial.printf("Showing eye%d.bmp on both eyes\n", n);
            break;
        }

        case '0':
            robotEyes.showIdentify();
            Serial.println("Eye identify: right=green R, left=orange L");
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
    // delay(2000);
    Serial.println("Starting...");
    Serial.printf("Free heap: %u, PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

    robotFace.begin();
    robotFace.showBootMessage("Checking health...");

#if defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL)
    scanI2cBus();
#endif

    robotFace.showBootMessage("Rubbing eyes...");
    robotEyes.begin();

    robotFace.showBootMessage("Stretching muscles...");
    if (!servoManager.begin()) {
        robotFace.showFatalError("PCA9685 servo board not found on I2C");
        robotEyes.showFatalError();
        Serial.println("PCA9685 missing — halting setup");
        while (true) delay(1000);  // FIXME watchdog will reset after 5s, so this isn't truly halting
    }
    robotMotion.begin();

#if defined(INA219_ADDR) && defined(PIN_I2C_SDA) && defined(PIN_I2C_SCL)
    robotFace.showBootMessage("Checking senses...");
    if (!currentSensor.begin(PIN_I2C_SDA, PIN_I2C_SCL)) {
        robotFace.showFatalError("INA219 current sensor not found on I2C");
        robotEyes.showFatalError();
        Serial.println("INA219 missing — halting setup");
        while (true) delay(1000);  // FIXME watchdog will reset after 5s, so this isn't truly halting
    }
#endif
    robotFace.showBootMessage("Checking radio waves...");
    webControl.begin();

    // Draw the face right here instead of relying on the first loop() tick
    // to clear the last boot message — keeps the panel in a deterministic
    // state before WiFi tasks start competing for SPI/CPU time.
    robotFace.showBootMessage("Ready!");
    // robotFace.update();

    // FIXME debugging
    // robotMotion.config.speedFactor = 0.25f;  // 0.1f;  // 10x slower for debugging

    // leg movement tuning
    robotMotion.config.liftFraction   = 0.67f;  // 0.33f;  // lift legs to 33% of their available range to speed up movement (no need to lift knees up to chest :-))
    robotMotion.config.crouchFraction = 0.95f;  // 1.0f is the most extreme crouch; reduce to avoid scraping knees on the ground

    // whole body movement tuning:
    // FIXME this is shared for both translation and rotation!!!
    robotMotion.config.actuateFraction = 0.5f;  // 1.0f;  // maximum length of the step

    // robotMotion.config.legLiftMs = 10;
    // robotMotion.config.actuateMs = 10;
    // robotMotion.config.legDropMs = 10;

    printHelp();
}

static void printCurrentConsumption() {
    if (!currentSensor.isReady()) return;
    static uint32_t lastMs = 0;
    uint32_t now           = millis();
    if (now - lastMs < kCurrentPrintIntervalMs) return;
    lastMs = now;
    currentSensor.read();
    Serial.printf("[INA219] %.2f V  %.1f mA  %.0f mW\n",
                  currentSensor.getBusVoltageV(),
                  currentSensor.getCurrentMa(),
                  currentSensor.getPowerMw());
}

void loop() {
    processSerialInput();
    servoMotion.update();
    robotMotion.update();
    webControl.update();
    printCurrentConsumption();

    // Show a Concentrating face while the robot is moving; restore the
    // prior expression once idle. Edge-detected so we don't fight 'm'.
    static bool wasIdle                            = true;
    static RobotFace::Expression preMoveExpression = RobotFace::Expression::Happy;
    bool idle                                      = robotMotion.isIdle();
    if (wasIdle && !idle) {
        preMoveExpression = robotFace.getExpression();
        robotFace.setExpression(RobotFace::Expression::Concentrating);
        robotEyes.setExpression(RobotFace::Expression::Concentrating);
    } else if (!wasIdle && idle) {
        robotFace.setExpression(preMoveExpression);
        robotEyes.setExpression(preMoveExpression);
    }
    wasIdle = idle;

    robotFace.update();
    robotEyes.update();
}
