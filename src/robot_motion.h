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
// A half-step is: lift one diagonal, drive the body servo to one extreme, drop
// the diagonal. The next half-step lifts the other diagonal and drives the
// body servo to the opposite extreme. Two half-steps make one full stride.
//
// All commands are queued and asynchronous; call update() from loop().
class RobotMotion {
public:
    struct Config {
        // Leg "down" angles (resting on ground) and "up" angles (lifted).
        // Left- and right-side parallelograms are usually mirrored, so left
        // legs lift toward higher angles and right legs toward lower ones.
        float legDownFL = 90.0f, legUpFL = 150.0f;
        float legDownFR = 90.0f, legUpFR = 30.0f;
        float legDownRL = 90.0f, legUpRL = 150.0f;
        float legDownRR = 90.0f, legUpRR = 30.0f;

        // Translation extremes. translationForward is the angle that, while
        // diagonal A is lifted, moves the robot physically forward; with
        // diagonal B lifted the opposite extreme is used to keep moving forward.
        float translationNeutral  = 90.0f;
        float translationForward  = 130.0f;
        float translationBackward = 50.0f;

        // Rotation extremes, by the same convention.
        float rotationNeutral = 90.0f;
        float rotationLeft    = 50.0f;
        float rotationRight   = 130.0f;

        // Crouch pose: all four legs to this angle.
        float crouchAngle = 60.0f;

        // Approximate yaw produced by one rotation half-step. Used to convert
        // rotate(degrees) into a half-step count; tune to your geometry.
        float degreesPerHalfRotation = 15.0f;

        // Phase durations (ms).
        uint32_t legLiftMs = 200;
        uint32_t actuateMs = 350;
        uint32_t legDropMs = 200;
        uint32_t poseMs    = 400;
    };

    explicit RobotMotion(ServoMotion& motion);

    Config config;

    // Drive every servo to its standing pose. Issues motion immediately.
    void begin();

    // Queued commands. Negative values reverse direction.
    void step(int fullSteps);    // +1 = one full stride forward
    void rotate(float degrees);  // + = left, - = right
    void crouch();
    void stand();

    void update();
    bool isIdle() const;

private:
    enum class Action : uint8_t { None,
                                  Walk,
                                  Rotate,
                                  Crouch,
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
    bool nextDiagonalA    = true;  // which diagonal lifts on the next half-step
    bool currentDiagonalA = true;  // diagonal lifted in the current half-step

    bool queueEmpty() const { return head == tail; }
    bool queueFull() const { return ((tail + 1) % kMaxJobs) == head; }
    void enqueue(const Job& j);

    void issueLift();
    void issueActuate();
    void issueDrop();
    void issuePose(Action action);

    float legUp(ServoId id) const;
    float legDown(ServoId id) const;
};
