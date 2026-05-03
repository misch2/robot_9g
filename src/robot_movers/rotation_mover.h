#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Rotates the robot using the Rotation servo. Mechanically distinct from
// HalfStepMover.
//
// Physical model: the chassis is rigidly attached to one diagonal (the
// "body diagonal"); the other diagonal is loose legs coupled to the
// chassis through the Rotation servo. The servo is one-sided: the angle
// between the two diagonals can only be 0 or +max, never negative.
//
// One full step = two half-steps:
//   1. Lift one diagonal, drive servo 0→+max.
//   2. Lift the other diagonal, drive servo +max→0.
//
// Body yaw only happens during the half-step where the *body* diagonal is
// in the air; the other half-step is purely realigning the loose legs.
// Net rotation per full step is `degreesPerRotation`, and direction is
// selected by which diagonal lifts first:
//
//   sign>0: diagA = {RearLeft, FrontRight} lifts first.
//   sign<0: diagB = {FrontLeft, RearRight} lifts first.
//
// The servo always starts and ends a job at 0, so no state persists
// across jobs. Within a half-step the same lift/actuate/drop overlap as
// HalfStepMover applies (bodyLeadInMs, bodySettleMs). Unlike
// HalfStepMover, half-steps *also* overlap each other: each half-step
// ends as soon as its body actuation has settled, and the leg drop
// continues into the next half-step's lift on the opposite diagonal.
// Both diagonals may briefly be airborne at the boundary. The final
// half-step of a job waits for legs to land before signalling done.
class RotationMover {
public:
    RotationMover(ServoMotion& motion, const RobotConfig& config);

    // |signedFullSteps| full steps; sign sets direction.
    void start(int signedFullSteps);

    // Returns true once the entire job has completed.
    bool update(uint32_t now);

private:
    void issueLift();
    void issueActuate();
    void issueDrop();
    void beginHalfStep(uint32_t now);

    ServoMotion& motion;
    const RobotConfig& config;

    int halfStepsRemaining = 0;
    bool currentDiagonalA  = false;
    bool servoAtExtreme    = false;
    bool active            = false;
    int directionSign      = 0;  // sign of the original signedFullSteps; used to recentre the head on completion

    uint32_t actuateAtMs   = 0;
    uint32_t dropAtMs      = 0;
    uint32_t halfStepEndMs = 0;
    bool actuateIssued     = false;
    bool dropIssued        = false;
};
