#include "robot_motion.h"
#include <math.h>

namespace {
constexpr ServoId kDiagA[2]   = {ServoId::RearLeft, ServoId::FrontRight};
constexpr ServoId kDiagB[2]   = {ServoId::FrontLeft, ServoId::RearRight};
constexpr ServoId kAllLegs[4] = {ServoId::FrontLeft, ServoId::FrontRight,
                                 ServoId::RearLeft, ServoId::RearRight};
// Clockwise viewed from above with the robot facing forward.
constexpr ServoId kClockwiseLegs[4] = {ServoId::FrontLeft, ServoId::FrontRight,
                                       ServoId::RearRight, ServoId::RearLeft};

constexpr float fractionBalanced = 0.0f;
constexpr float fractionMax      = 1.0f;
constexpr float fractionMin      = -1.0f;

}  // namespace

RobotMotion::RobotMotion(ServoMotion& m) : motion(m) {}

void RobotMotion::begin() {
    // Start in a neutral standing pose, every servo in .
    for (ServoId leg : kAllLegs) motion.moveToFraction(leg, fractionBalanced, config.poseMs);
    motion.moveToFraction(ServoId::Translation, fractionBalanced, config.poseMs);
    motion.moveToFraction(ServoId::Rotation, fractionBalanced, config.poseMs);
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

void RobotMotion::sit() { enqueue({Action::Sit, 0}); }
void RobotMotion::crouch() { enqueue({Action::Crouch, 0}); }
void RobotMotion::stand() { enqueue({Action::Stand, 0}); }

void RobotMotion::dance(int rotations) {
    if (rotations <= 0) return;
    enqueue({Action::Dance, rotations * 4});
}

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
    motion.moveToFraction(pair[0], fractionBalanced, config.legDropMs);
    motion.moveToFraction(pair[1], fractionBalanced, config.legDropMs);
}

void RobotMotion::issueActuate() {
    // sign +1 = forward / left (the "primary" job direction), -1 = reverse.
    // When diagonalA is lifted, primary job direction maps to +actuateFraction;
    // when diagonalB is lifted the body servo must go the opposite way to keep
    // pushing the body the same way. Settling targets 0 to recenter the body
    // actuator while a diagonal is still lifted (no friction fight).
    float fraction;
    if (settling) {
        fraction = fractionBalanced;
    } else {
        const int sign = (currentJob.remaining > 0) ? 1 : -1;
        fraction       = (((sign > 0) == currentDiagonalA) ? fractionMax : fractionMin) * config.actuateFraction;
    }
    const ServoId target = (currentJob.action == Action::Walk) ? ServoId::Translation : ServoId::Rotation;
    motion.moveToFraction(target, fraction, config.actuateMs);
}

void RobotMotion::issuePose(Action action) {
    if (action == Action::Sit) {
        // Sit = back on the ground, front raised. Only the front legs lift the body; the rear legs just tuck under it. Translation and rotation are neutral.
        motion.moveToFraction(ServoId::RearLeft, fractionMax, config.poseMs);
        motion.moveToFraction(ServoId::RearRight, fractionMax, config.poseMs);
        motion.moveToFraction(ServoId::FrontLeft, fractionBalanced, config.poseMs);
        motion.moveToFraction(ServoId::FrontRight, fractionBalanced, config.poseMs);
        motion.moveToFraction(ServoId::Translation, fractionBalanced, config.poseMs);
        motion.moveToFraction(ServoId::Rotation, fractionBalanced, config.poseMs);
        return;
    }

    const float legFraction = (action == Action::Crouch) ? config.crouchFraction : fractionBalanced;
    for (ServoId leg : kAllLegs) motion.moveToFraction(leg, legFraction, config.poseMs);
    if (action != Action::Crouch) {
        motion.moveToFraction(ServoId::Translation, fractionBalanced, config.poseMs);
        motion.moveToFraction(ServoId::Rotation, fractionBalanced, config.poseMs);
    }
}

void RobotMotion::startDanceStep() {
    const ServoId leg = kClockwiseLegs[danceLegIdx & 3];
    motion.moveToFraction(leg, config.liftFraction, config.legLiftMs);

    const uint32_t now = millis();
    danceDropAtMs      = now + config.legLiftMs + config.danceHoldMs;
    danceStepEndMs     = danceDropAtMs + config.legDropMs;
    danceDropIssued    = false;
    phase              = Phase::DanceStep;
}

void RobotMotion::startHalfStep() {
    currentDiagonalA = nextDiagonalA;
    issueLift();

    const uint32_t now = millis();
    // Half-step length is whichever constraint is tighter: legs need
    // legLiftMs + legDropMs, body needs bodyLeadInMs + actuateMs + bodySettleMs.
    const uint32_t legSpan  = config.legLiftMs + config.legDropMs;
    const uint32_t bodySpan = config.bodyLeadInMs + config.actuateMs + config.bodySettleMs;
    const uint32_t total    = legSpan > bodySpan ? legSpan : bodySpan;

    actuateAtMs   = now + config.bodyLeadInMs;
    halfStepEndMs = now + total;
    dropAtMs      = halfStepEndMs - config.legDropMs;
    actuateIssued = false;
    dropIssued    = false;
    phase         = Phase::HalfStep;
}

void RobotMotion::update() {
    const uint32_t now = millis();

    if (phase == Phase::HalfStep) {
        // Fire the queued sub-motions as their scheduled times arrive.
        // Signed comparison handles millis() wraparound safely.
        if (!actuateIssued && (int32_t)(now - actuateAtMs) >= 0) {
            issueActuate();
            actuateIssued = true;
        }
        if (!dropIssued && (int32_t)(now - dropAtMs) >= 0) {
            issueDrop();
            dropIssued = true;
        }
        if ((int32_t)(now - halfStepEndMs) < 0) return;

        // Half-step complete.
        nextDiagonalA = !currentDiagonalA;
        if (settling) {
            settling          = false;
            currentJob.action = Action::None;
        } else {
            if (currentJob.remaining > 0)
                currentJob.remaining--;
            else if (currentJob.remaining < 0)
                currentJob.remaining++;
            if (currentJob.remaining == 0) {
                // Run one more lift / actuate(0) / drop to recenter the body
                // actuator while a diagonal is lifted, so a subsequent reversal
                // doesn't waste its first half-step.
                settling = true;
            }
        }
        phase = Phase::Idle;
    } else if (phase == Phase::DanceStep) {
        const ServoId leg = kClockwiseLegs[danceLegIdx & 3];
        if (!danceDropIssued && (int32_t)(now - danceDropAtMs) >= 0) {
            motion.moveToFraction(leg, fractionBalanced, config.legDropMs);
            danceDropIssued = true;
        }
        if ((int32_t)(now - danceStepEndMs) < 0) return;
        danceLegIdx = (danceLegIdx + 1) & 3;
        if (currentJob.remaining > 0) currentJob.remaining--;
        if (currentJob.remaining == 0) currentJob.action = Action::None;
        phase = Phase::Idle;
    } else if (phase == Phase::Pose) {
        if (!motion.isIdle()) return;
        phase             = Phase::Idle;
        currentJob.action = Action::None;
    }

    // Decide what to start next.
    if (currentJob.action == Action::Walk || currentJob.action == Action::Rotate) {
        startHalfStep();
        return;
    }
    if (currentJob.action == Action::Dance) {
        startDanceStep();
        return;
    }
    if (queueEmpty()) return;
    currentJob = queue[head];
    head       = (head + 1) % kMaxJobs;
    switch (currentJob.action) {
        case Action::Walk:
        case Action::Rotate:
            startHalfStep();
            break;
        case Action::Dance:
            danceLegIdx = 0;
            startDanceStep();
            break;
        case Action::Crouch:
        case Action::Stand:
        case Action::Sit:
            issuePose(currentJob.action);
            phase = Phase::Pose;
            break;
        case Action::None:
            break;
    }
}
