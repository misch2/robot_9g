#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Rotates the robot using the Rotation servo (mounted on diagA = {RearLeft,
// FrontRight}). Mechanically distinct from HalfStepMover.
//
// The Rotation servo is one-sided: the diagA body half can only be tilted
// to one side of the diagB body half, so its target only ever alternates
// between 0 and +max. Direction is selected by which diagonal lifts first:
//
//   sign>0 (rotate one way):   first lift is diagA. Lifting A while servo
//                              drives 0→+max swings the in-air diagA body
//                              half +N relative to planted diagB; next
//                              half-step lifts diagB and servo recenters
//                              +max→0, swinging diagB body half to align.
//
//   sign<0 (rotate other way): first lift is diagB. Lifting B while servo
//                              drives 0→+max swings the in-air diagB body
//                              half −N relative to planted diagA; next
//                              half-step lifts diagA and servo recenters.
//
// Each half-step is useful. For even half-step counts the natural
// alternation ends with the servo at 0; odd counts leave it at +max and
// the next rotate job continues the alternation from there.
//
// Same overlap timing as HalfStepMover (bodyLeadInMs, bodySettleMs).
class RotationMover {
public:
    RotationMover(ServoMotion& motion, const RobotConfig& config);

    // |signedHalfSteps| half-steps; sign sets direction.
    void start(int signedHalfSteps);

    // Returns true once the entire job has completed.
    bool update(uint32_t now);

private:
    void issueLift();
    void issueActuate();
    void issueDrop();
    void beginHalfStep(uint32_t now);

    ServoMotion& motion;
    const RobotConfig& config;

    int remaining         = 0;
    bool currentDiagonalA = false;
    bool active           = false;
    // Persists across jobs: tracks whether the rotation servo is currently
    // at +max (true) or 0 (false). Toggled at the end of each half-step so
    // the next rotate job continues the alternation cleanly even after odd
    // half-step counts.
    bool servoAtExtreme = false;

    uint32_t actuateAtMs   = 0;
    uint32_t dropAtMs      = 0;
    uint32_t halfStepEndMs = 0;
    bool actuateIssued     = false;
    bool dropIssued        = false;
};
