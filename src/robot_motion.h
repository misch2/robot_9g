#pragma once

#include <Arduino.h>
#include "config.h"
#include "robot_movers/dance_mover.h"
#include "robot_movers/half_step_mover.h"
#include "robot_movers/head_look_around_mover.h"
#include "robot_movers/head_nod_mover.h"
#include "robot_movers/head_shake_mover.h"
#include "robot_movers/pose_mover.h"
#include "robot_movers/robot_config.h"
#include "robot_movers/rotation_mover.h"
#include "servo_motion.h"

// Whole-robot movements layered on top of ServoMotion.
//
// Mechanics: four leg servos lift/lower each leg via a parallelogram. Two body
// servos couple diagonal leg pairs along the body axis (Translation) and in
// yaw (Rotation). Diagonal A = {RearLeft, FrontRight}, diagonal B = {FrontLeft,
// RearRight}; the body servos always move A and B in opposite mechanical
// directions, so the alternation between half-steps is what produces net body
// motion.
//
// Per-servo direction and HW limits live in kServos[] (config.h); pose values
// here are unitless fractions in [-1, +1] passed to ServoMotion::moveToFraction.
// 0.0 = restAngle, +1.0 = primary safety limit, -1.0 = the opposite limit.
//
// A half-step is: lift one diagonal, drive the body servo to one extreme, drop
// the diagonal. The next half-step lifts the other diagonal and drives the
// body servo to the opposite extreme. Two half-steps make one full stride.
//
// The three sub-motions overlap on a fixed schedule: body actuation begins
// `bodyLeadInMs` after the lift starts (so the legs are already partially
// raised), and the drop is timed so the legs touch down `bodySettleMs` after
// the body finishes moving. The legs and body servos are independent, so
// overlapping is safe and the gait is much smoother than a strictly serial
// lift→actuate→drop sequence.
//
// All commands are queued and asynchronous; call update() from loop().
//
// This class is a thin orchestrator: it owns the job queue and dispatches
// each job to one of three independent state machines (HalfStepMover,
// DanceMover, PoseMover) declared in robot_movers.h.
class RobotMotion {
public:
    explicit RobotMotion(ServoMotion& motion);

    // Tunables exposed for runtime tweaking from the serial console.
    RobotConfig config;

    // Drive every servo to its standing pose. Treats it as a regular pose job.
    void begin();

    // Queued commands. Negative values reverse direction.
    void step(int fullSteps);    // +1 = one full stride forward
    void rotate(float degrees);  // + = left, - = right
    void sit();
    void crouch();
    void stand();
    void dance(int rotations = 1);  // lift+drop each leg clockwise; one rotation = 4 legs
    void shakeNo(int shakes = 4);   // "no" head gesture; one shake = one side-to-side transition
    void nodYes(int nods = 4);      // "yes" head gesture; one nod = one up-to-down transition
    // Sweep both head servos as sine waves. Pattern Circular = pan & tilt 90°
    // out of phase (circular gaze); Figure8 = Lissajous 2:1 (figure-eight).
    void lookAround(int cycles     = 2,
                    HeadLookAroundMover::Pattern pattern = HeadLookAroundMover::Pattern::Circular);

    void update();
    bool isIdle() const;

    // Which mover is currently driving the servos (None when idle between
    // jobs). Exposed so callers can pick a matching facial expression for
    // the active gesture.
    enum class Active : uint8_t { None,
                                  HalfStep,
                                  Rotation,
                                  Dance,
                                  Pose,
                                  HeadShake,
                                  HeadNod,
                                  HeadLookAround };
    Active getActive() const { return active; }

private:
    enum class Action : uint8_t { None,
                                  Walk,
                                  Rotate,
                                  Sit,
                                  Crouch,
                                  Stand,
                                  Dance,
                                  HeadShake,
                                  HeadNod,
                                  HeadLookAroundCircular,
                                  HeadLookAroundFigure8 };

    static constexpr size_t kMaxJobs = 8;

    struct Job {
        Action action = Action::None;
        int param     = 0;  // signed half-steps for Walk; signed full steps for Rotate; leg count for Dance; 0 for poses
        Job()         = default;
        Job(Action a) : action(a), param(0) {}
        Job(Action a, int p) : action(a), param(p) {}
    };

    ServoMotion& motion;
    HalfStepMover halfStepMover;
    RotationMover rotationMover;
    DanceMover danceMover;
    PoseMover poseMover;
    HeadShakeMover headShakeMover;
    HeadNodMover headNodMover;
    HeadLookAroundMover headLookAroundMover;
    Active active = Active::None;

    Job queue[kMaxJobs];
    size_t head = 0;
    size_t tail = 0;

    bool queueEmpty() const { return head == tail; }
    bool queueFull() const { return ((tail + 1) % kMaxJobs) == head; }
    void enqueue(const Job& j);
    void startJob(const Job& j);
};
