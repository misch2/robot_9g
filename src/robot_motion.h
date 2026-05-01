#pragma once

#include <Arduino.h>
#include "config.h"
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
// All commands are queued and asynchronous; call update() from loop().
class RobotMotion {
public:
    struct Config {
        // How far each leg lifts during the gait, as a fraction of its
        // available range from rest toward the primary limit. 1.0 would lift
        // to the safety limit; the gait usually wants less than that.
        float liftFraction = 0.67f;

        // Crouch pose: how far each leg bends in its lift direction. 1.0 =
        // pinned at the safety limit (the most extreme crouch the HW allows).
        float crouchFraction = 1.0f;

        // Body servo amplitude during a half-step actuation. Translation goes
        // ±actuateFraction to step forward/backward; Rotation does the same
        // for left/right.
        float actuateFraction = 0.44f;

        // Approximate yaw produced by one rotation half-step. Used to convert
        // rotate(degrees) into a half-step count; tune to your geometry.
        float degreesPerHalfRotation = 15.0f;

        // Phase durations (ms).
        uint32_t legLiftMs = 200;
        uint32_t actuateMs = 350;
        uint32_t legDropMs = 400;  // 200;
        uint32_t poseMs    = 400;
    };

    explicit RobotMotion(ServoMotion& motion);

    Config config;

    // Drive every servo to its standing pose. Issues motion immediately.
    void begin();

    // Queued commands. Negative values reverse direction.
    void step(int fullSteps);    // +1 = one full stride forward
    void rotate(float degrees);  // + = left, - = right
    void sit();
    void crouch();
    void stand();

    void update();
    bool isIdle() const;

private:
    enum class Action : uint8_t { None,
                                  Walk,
                                  Rotate,
                                  Crouch,
                                  Sit,
                                  Stand };
    enum class Phase : uint8_t { Idle,
                                 Lift,
                                 Actuate,
                                 Drop,
                                 Pose };

    static constexpr size_t kMaxJobs = 8;

    struct Job {
        Action action = Action::None;
        int remaining = 0;  // signed half-step count; sign = direction
        Job()         = default;
        Job(Action a, int r) : action(a), remaining(r) {}
    };

    ServoMotion& motion;
    Job queue[kMaxJobs];
    size_t head = 0;
    size_t tail = 0;
    Job currentJob;
    Phase phase           = Phase::Idle;
    bool nextDiagonalA    = true;   // which diagonal lifts on the next half-step
    bool currentDiagonalA = true;   // diagonal lifted in the current half-step
    bool settling         = false;  // running the recenter half-step at job end

    bool queueEmpty() const { return head == tail; }
    bool queueFull() const { return ((tail + 1) % kMaxJobs) == head; }
    void enqueue(const Job& j);

    void issueLift();
    void issueActuate();
    void issueDrop();
    void issuePose(Action action);
};
