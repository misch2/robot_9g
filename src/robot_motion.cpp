#include "robot_motion.h"
#include <math.h>

namespace {
constexpr ServoId kDiagA[2] = {ServoId::RearLeft, ServoId::FrontRight};
constexpr ServoId kDiagB[2] = {ServoId::FrontLeft, ServoId::RearRight};
}  // namespace

RobotMotion::RobotMotion(ServoMotion& m) : motion(m) {}

void RobotMotion::begin() {
    motion.moveTo(ServoId::FrontLeft, config.legDownFL, config.poseMs);
    motion.moveTo(ServoId::FrontRight, config.legDownFR, config.poseMs);
    motion.moveTo(ServoId::RearLeft, config.legDownRL, config.poseMs);
    motion.moveTo(ServoId::RearRight, config.legDownRR, config.poseMs);
    motion.moveTo(ServoId::Translation, config.translationNeutral, config.poseMs);
    motion.moveTo(ServoId::Rotation, config.rotationNeutral, config.poseMs);
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

float RobotMotion::legUp(ServoId id) const {
    switch (id) {
        case ServoId::FrontLeft:
            return config.legUpFL;
        case ServoId::FrontRight:
            return config.legUpFR;
        case ServoId::RearLeft:
            return config.legUpRL;
        case ServoId::RearRight:
            return config.legUpRR;
        default:
            return 0.0f;
    }
}

float RobotMotion::legDown(ServoId id) const {
    switch (id) {
        case ServoId::FrontLeft:
            return config.legDownFL;
        case ServoId::FrontRight:
            return config.legDownFR;
        case ServoId::RearLeft:
            return config.legDownRL;
        case ServoId::RearRight:
            return config.legDownRR;
        default:
            return 0.0f;
    }
}

void RobotMotion::issueLift() {
    const ServoId* pair = currentDiagonalA ? kDiagA : kDiagB;
    motion.moveTo(pair[0], legUp(pair[0]), config.legLiftMs);
    motion.moveTo(pair[1], legUp(pair[1]), config.legLiftMs);
}

void RobotMotion::issueDrop() {
    const ServoId* pair = currentDiagonalA ? kDiagA : kDiagB;
    motion.moveTo(pair[0], legDown(pair[0]), config.legDropMs);
    motion.moveTo(pair[1], legDown(pair[1]), config.legDropMs);
}

void RobotMotion::issueActuate() {
    // sign +1 = forward / left (the "primary" direction), -1 = backward / right.
    // When diagonalA is lifted, primary direction maps to the primary extreme;
    // when diagonalB is lifted, the body servo must travel to the opposite
    // extreme to keep pushing the body the same way.
    const int sign     = (currentJob.remaining > 0) ? 1 : -1;
    const bool primary = ((sign > 0) == currentDiagonalA);
    if (currentJob.action == Action::Walk) {
        const float angle = primary ? config.translationForward : config.translationBackward;
        motion.moveTo(ServoId::Translation, angle, config.actuateMs);
    } else {
        const float angle = primary ? config.rotationLeft : config.rotationRight;
        motion.moveTo(ServoId::Rotation, angle, config.actuateMs);
    }
}

void RobotMotion::issuePose(Action action) {
    if (action == Action::Crouch) {
        motion.moveTo(ServoId::FrontLeft, config.crouchAngle, config.poseMs);
        motion.moveTo(ServoId::FrontRight, config.crouchAngle, config.poseMs);
        motion.moveTo(ServoId::RearLeft, config.crouchAngle, config.poseMs);
        motion.moveTo(ServoId::RearRight, config.crouchAngle, config.poseMs);
    } else {
        motion.moveTo(ServoId::FrontLeft, config.legDownFL, config.poseMs);
        motion.moveTo(ServoId::FrontRight, config.legDownFR, config.poseMs);
        motion.moveTo(ServoId::RearLeft, config.legDownRL, config.poseMs);
        motion.moveTo(ServoId::RearRight, config.legDownRR, config.poseMs);
        motion.moveTo(ServoId::Translation, config.translationNeutral, config.poseMs);
        motion.moveTo(ServoId::Rotation, config.rotationNeutral, config.poseMs);
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
