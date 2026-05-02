#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Walk via half-steps on the Translation servo: lift one diagonal pair,
// drive Translation to one extreme, drop. The next half-step lifts the
// other diagonal and drives Translation to the opposite extreme. Two
// half-steps make one full stride.
//
// At the end of the requested half-steps the body actuator is left at one
// extreme; recentering is not automatic so the orchestrator can chain
// same-direction Walk jobs without a wasteful recenter between them. Call
// startRecenter() (a single half-step that lifts the other diagonal and
// drives Translation to 0) once the gait actually needs to settle.
//
// Rotation uses a different pattern and lives in RotationMover.
class HalfStepMover {
public:
    HalfStepMover(ServoMotion& motion, const RobotConfig& config);

    // |signedHalfSteps| half-steps; sign sets direction (forward/back).
    void start(int signedHalfSteps);

    // Run a single recenter half-step that drives Translation back to 0
    // while a diagonal is lifted (no friction fight). Only meaningful when
    // needsRecenter() is true.
    void startRecenter();

    // Returns true once the currently active phase (regular run or
    // recenter) has completed.
    bool update(uint32_t now);

    // True after a regular run has ended with the body actuator off-center.
    // Cleared once startRecenter() completes.
    bool needsRecenter() const { return bodyOffCenter; }

    // Sign of the most recently started regular job: +1 forward, -1 back,
    // 0 if none ever started. Used by the orchestrator to coalesce
    // same-direction Walks.
    int direction() const { return lastDir; }

private:
    void issueLift();
    void issueActuate();
    void issueDrop();
    void beginHalfStep(uint32_t now);

    ServoMotion& motion;
    const RobotConfig& config;

    int remaining = 0;
    int lastDir   = 0;
    // Diagonal alternation persists across jobs so a reversal continues on
    // the diagonal that wasn't just lifted.
    bool currentDiagonalA = true;
    bool nextDiagonalA    = true;
    bool settling         = false;
    bool active           = false;
    bool bodyOffCenter    = false;

    uint32_t actuateAtMs   = 0;
    uint32_t dropAtMs      = 0;
    uint32_t halfStepEndMs = 0;
    bool actuateIssued     = false;
    bool dropIssued        = false;
};
