#include "robot_motion.h"
#include <math.h>

namespace {
constexpr ServoId kDiagA[2]   = {ServoId::RearLeft, ServoId::FrontRight};
constexpr ServoId kDiagB[2]   = {ServoId::FrontLeft, ServoId::RearRight};
constexpr ServoId kAllLegs[4] = {ServoId::FrontLeft, ServoId::FrontRight,
                                 ServoId::RearLeft, ServoId::RearRight};
}  // namespace

RobotMotion::RobotMotion(ServoMotion& m) : motion(m) {}

void RobotMotion::begin() {
    // FIXME use "rest" position, is it really always 0.0f? Maybe add a helper in ServoMotion to convert from fraction to angle using the ServoSpec?
    for (ServoId leg : kAllLegs) motion.moveToFraction(leg, 0.0f, config.poseMs);
    motion.moveToFraction(ServoId::Translation, 0.0f, config.poseMs);
    motion.moveToFraction(ServoId::Rotation, 0.0f, config.poseMs);
}

void RobotMotion::enqueue(const Job& j) {
    if (queueFull()) {
        Serial.println("RobotMotion: job queue full, dropping command");
        return;
    }
    queue[tail] = j;
    tail        = (tail + 1) % kMaxJobs;
}

void RobotMotion::step(int fullSteps) {
    if (fullSteps == 0) return;
    enqueue({Action::Walk, fullSteps * 2});
}

void RobotMotion::rotate(float degrees) {
    int halfSteps = (int)lroundf(degrees / config.degreesPerHalfRotation);
    if (halfSteps == 0) return;
    enqueue({Action::Rotate, halfSteps});
}

void RobotMotion::crouch() { enqueue({Action::Crouch, 0}); }
void RobotMotion::stand() { enqueue({Action::Stand, 0}); }

bool RobotMotion::isIdle() const {
    return phase == Phase::Idle && currentJob.action == Action::None && queueEmpty();
}

void RobotMotion::issueLift() {
    const ServoId* pair = currentDiagonalA ? kDiagA : kDiagB;
    motion.moveToFraction(pair[0], config.liftFraction, config.legLiftMs);
    motion.moveToFraction(pair[1], config.liftFraction, config.legLiftMs);
}

void RobotMotion::issueDrop() {
    const ServoId* pair = currentDiagonalA ? kDiagA : kDiagB;
    motion.moveToFraction(pair[0], 0.0f, config.legDropMs);
    motion.moveToFraction(pair[1], 0.0f, config.legDropMs);
}

void RobotMotion::issueActuate() {
    // sign +1 = forward / left (the "primary" job direction), -1 = reverse.
    // When diagonalA is lifted, primary job direction maps to +actuateFraction;
    // when diagonalB is lifted the body servo must go the opposite way to keep
    // pushing the body the same way.
    const int sign       = (currentJob.remaining > 0) ? 1 : -1;
    const float fraction = (((sign > 0) == currentDiagonalA) ? +1.0f : -1.0f) * config.actuateFraction;
    const ServoId target = (currentJob.action == Action::Walk) ? ServoId::Translation : ServoId::Rotation;
    motion.moveToFraction(target, fraction, config.actuateMs);
}

void RobotMotion::issuePose(Action action) {
    const float legFraction = (action == Action::Crouch) ? config.crouchFraction : 0.0f;
    for (ServoId leg : kAllLegs) motion.moveToFraction(leg, legFraction, config.poseMs);
    if (action != Action::Crouch) {
        motion.moveToFraction(ServoId::Translation, 0.0f, config.poseMs);
        motion.moveToFraction(ServoId::Rotation, 0.0f, config.poseMs);
    }
}

void RobotMotion::update() {
    if (!motion.isIdle()) return;

    while (true) {
        // Wrap up the phase that just completed.
        switch (phase) {
            case Phase::Lift:
                issueActuate();
                phase = Phase::Actuate;
                return;
            case Phase::Actuate:
                issueDrop();
                phase = Phase::Drop;
                return;
            case Phase::Drop:
                if (currentJob.remaining > 0)
                    currentJob.remaining--;
                else if (currentJob.remaining < 0)
                    currentJob.remaining++;
                nextDiagonalA = !currentDiagonalA;
                phase         = Phase::Idle;
                if (currentJob.remaining == 0) currentJob.action = Action::None;
                break;
            case Phase::Pose:
                phase             = Phase::Idle;
                currentJob.action = Action::None;
                break;
            case Phase::Idle:
                break;
        }

        // Decide what to start next.
        if (currentJob.action == Action::Walk || currentJob.action == Action::Rotate) {
            currentDiagonalA = nextDiagonalA;
            issueLift();
            phase = Phase::Lift;
            return;
        }
        if (queueEmpty()) return;
        currentJob = queue[head];
        head       = (head + 1) % kMaxJobs;
        if (currentJob.action == Action::Crouch || currentJob.action == Action::Stand) {
            issuePose(currentJob.action);
            phase = Phase::Pose;
            return;
        }
        // Walk / Rotate: loop back around to issue the lift.
    }
}
