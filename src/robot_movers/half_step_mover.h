#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Lift one diagonal pair, drive a body servo to one extreme, drop the pair.
// The next half-step lifts the other diagonal and drives the body servo to
// the opposite extreme. Two half-steps make one full stride.
//
// Walk and Rotate are the same machine; only the body servo differs
// (Translation vs Rotation), so it's a parameter to start().
class HalfStepMover {
public:
    HalfStepMover(ServoMotion& motion, const RobotConfig& config);

    // |signedHalfSteps| half-steps; sign sets direction (forward/back for
    // Translation, left/right for Rotation).
    void start(ServoId bodyServo, int signedHalfSteps);

    // Returns true once the entire job (including the final settling
    // half-step that recenters the body actuator) has completed.
    bool update(uint32_t now);

private:
    void issueLift();
    void issueActuate();
    void issueDrop();
    void beginHalfStep(uint32_t now);

    ServoMotion& motion;
    const RobotConfig& config;

    ServoId bodyServo = ServoId::Translation;
    int remaining     = 0;
    // Diagonal alternation persists across jobs so a reversal continues on
    // the diagonal that wasn't just lifted.
    bool currentDiagonalA = true;
    bool nextDiagonalA    = true;
    bool settling         = false;
    bool active           = false;

    uint32_t actuateAtMs   = 0;
    uint32_t dropAtMs      = 0;
    uint32_t halfStepEndMs = 0;
    bool actuateIssued     = false;
    bool dropIssued        = false;
};
