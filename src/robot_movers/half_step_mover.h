#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Walk via half-steps on the Translation servo: lift one diagonal pair,
// drive Translation to one extreme, drop. The next half-step lifts the
// other diagonal and drives Translation to the opposite extreme. Two
// half-steps make one full stride. After the requested count, a final
// settling half-step recenters Translation to 0.
//
// Rotation uses a different pattern and lives in RotationMover.
class HalfStepMover {
public:
    HalfStepMover(ServoMotion& motion, const RobotConfig& config);

    // |signedHalfSteps| half-steps; sign sets direction (forward/back).
    void start(int signedHalfSteps);

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

    int remaining = 0;
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
