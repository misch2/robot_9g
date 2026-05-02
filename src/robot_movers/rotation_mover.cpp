#include "rotation_mover.h"

namespace {
constexpr ServoId kDiagA[2] = {ServoId::RearLeft, ServoId::FrontRight};
constexpr ServoId kDiagB[2] = {ServoId::FrontLeft, ServoId::RearRight};
}  // namespace

RotationMover::RotationMover(ServoMotion& m, const RobotConfig& c) : motion(m), config(c) {}

void RotationMover::start(int signedHalfSteps) {
    if (signedHalfSteps == 0) return;
    remaining = signedHalfSteps;
    // Direction is encoded in lift order. With the servo currently at 0
    // (servoAtExtreme=false), sign>0 lifts diagA first, sign<0 lifts diagB
    // first. With the servo at +max (after an odd-count rotation), the
    // first lift is the inverse of that — XOR encodes both cases.
    const bool positive = signedHalfSteps > 0;
    currentDiagonalA    = positive != servoAtExtreme;
    active              = true;
    beginHalfStep(millis());
}

void RotationMover::beginHalfStep(uint32_t now) {
    issueLift();

    const uint32_t legSpan  = config.scaled(config.legLiftMs) + config.scaled(config.legDropMs);
    const uint32_t bodySpan = config.scaled(config.bodyLeadInMs) + config.scaled(config.actuateMs) + config.scaled(config.bodySettleMs);
    const uint32_t total    = legSpan > bodySpan ? legSpan : bodySpan;

    actuateAtMs   = now + config.scaled(config.bodyLeadInMs);
    halfStepEndMs = now + total;
    dropAtMs      = halfStepEndMs - config.scaled(config.legDropMs);
    actuateIssued = false;
    dropIssued    = false;
}

bool RotationMover::update(uint32_t now) {
    if (!active) return true;

    if (!actuateIssued && (int32_t)(now - actuateAtMs) >= 0) {
        issueActuate();
        actuateIssued = true;
    }
    if (!dropIssued && (int32_t)(now - dropAtMs) >= 0) {
        issueDrop();
        dropIssued = true;
    }
    if ((int32_t)(now - halfStepEndMs) < 0) return false;

    // Half-step complete: the servo is now at the position issueActuate
    // drove it to, and the next half-step lifts the other diagonal.
    servoAtExtreme   = !servoAtExtreme;
    currentDiagonalA = !currentDiagonalA;

    if (remaining > 0)
        remaining--;
    else if (remaining < 0)
        remaining++;

    if (remaining == 0) {
        active = false;
        return true;
    }
    beginHalfStep(now);
    return false;
}

void RotationMover::issueLift() {
    const ServoId* pair  = currentDiagonalA ? kDiagA : kDiagB;
    const uint32_t durMs = config.scaled(config.legLiftMs);
    motion.moveToFraction(pair[0], config.liftFraction, durMs);
    motion.moveToFraction(pair[1], config.liftFraction, durMs);
}

void RotationMover::issueDrop() {
    const ServoId* pair  = currentDiagonalA ? kDiagA : kDiagB;
    const uint32_t durMs = config.scaled(config.legDropMs);
    motion.moveToFraction(pair[0], fractionBalanced, durMs);
    motion.moveToFraction(pair[1], fractionBalanced, durMs);
}

void RotationMover::issueActuate() {
    // The rotation servo is mechanically one-sided: it only moves between 0
    // and +max. Each half-step toggles it to the opposite of its current
    // position; direction is set by which diagonal is lifted (see start()).
    const float fraction = servoAtExtreme ? fractionBalanced : fractionMax * config.actuateFraction;
    motion.moveToFraction(ServoId::Rotation, fraction, config.scaled(config.actuateMs));
}
