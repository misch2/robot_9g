#include "half_step_mover.h"

namespace {
constexpr ServoId kDiagA[2] = {ServoId::RearLeft, ServoId::FrontRight};
constexpr ServoId kDiagB[2] = {ServoId::FrontLeft, ServoId::RearRight};
}  // namespace

HalfStepMover::HalfStepMover(ServoMotion& m, const RobotConfig& c) : motion(m), config(c) {}

void HalfStepMover::start(int signedHalfSteps) {
    if (signedHalfSteps == 0) return;
    remaining = signedHalfSteps;
    lastDir   = (signedHalfSteps > 0) ? 1 : -1;
    settling  = false;
    active    = true;
    beginHalfStep(millis());
}

void HalfStepMover::startRecenter() {
    if (!bodyOffCenter) return;
    settling = true;
    active   = true;
    beginHalfStep(millis());
}

void HalfStepMover::beginHalfStep(uint32_t now) {
    currentDiagonalA = nextDiagonalA;
    issueLift();

    // Half-step length is whichever constraint is tighter: legs need
    // legLiftMs + legDropMs, body needs bodyLeadInMs + actuateMs + bodySettleMs.
    const uint32_t legSpan  = config.scaled(config.legLiftMs) + config.scaled(config.legDropMs);
    const uint32_t bodySpan = config.scaled(config.bodyLeadInMs) + config.scaled(config.actuateMs) + config.scaled(config.bodySettleMs);
    const uint32_t total    = legSpan > bodySpan ? legSpan : bodySpan;

    actuateAtMs   = now + config.scaled(config.bodyLeadInMs);
    halfStepEndMs = now + total;
    dropAtMs      = halfStepEndMs - config.scaled(config.legDropMs);
    actuateIssued = false;
    dropIssued    = false;
}

bool HalfStepMover::update(uint32_t now) {
    if (!active) return true;

    // Fire the queued sub-motions as their scheduled times arrive. Signed
    // comparison handles millis() wraparound safely.
    if (!actuateIssued && (int32_t)(now - actuateAtMs) >= 0) {
        issueActuate();
        actuateIssued = true;
    }
    if (!dropIssued && (int32_t)(now - dropAtMs) >= 0) {
        issueDrop();
        dropIssued = true;
    }
    if ((int32_t)(now - halfStepEndMs) < 0) return false;

    // Half-step complete.
    nextDiagonalA = !currentDiagonalA;

    if (settling) {
        settling      = false;
        active        = false;
        bodyOffCenter = false;
        return true;
    }

    if (remaining > 0)
        remaining--;
    else if (remaining < 0)
        remaining++;

    if (remaining == 0) {
        // Hand control back to the orchestrator with the body actuator at
        // one extreme. If the next queued job is a same-direction Walk it
        // can dispatch directly (alternation continues). Otherwise it must
        // call startRecenter() to drive Translation back to 0 while a
        // diagonal is lifted, so a subsequent reversal or pose doesn't
        // waste its first half-step / fight friction.
        active        = false;
        bodyOffCenter = true;
        return true;
    }
    beginHalfStep(now);
    return false;
}

void HalfStepMover::issueLift() {
    const ServoId* pair  = currentDiagonalA ? kDiagA : kDiagB;
    const uint32_t durMs = config.scaled(config.legLiftMs);
    motion.moveToFraction(pair[0], config.liftFraction, durMs);
    motion.moveToFraction(pair[1], config.liftFraction, durMs);
}

void HalfStepMover::issueDrop() {
    const ServoId* pair  = currentDiagonalA ? kDiagA : kDiagB;
    const uint32_t durMs = config.scaled(config.legDropMs);
    motion.moveToFraction(pair[0], fractionBalanced, durMs);
    motion.moveToFraction(pair[1], fractionBalanced, durMs);
}

void HalfStepMover::issueActuate() {
    // sign +1 = forward, -1 = backward.
    // When diagonalA is lifted, primary direction maps to +actuateFraction;
    // when diagonalB is lifted the body servo must go the opposite way to
    // keep pushing the body the same direction. Settling targets 0 to
    // recenter the body actuator while a diagonal is still lifted (no
    // friction fight).
    float fraction;
    if (settling) {
        fraction = fractionBalanced;
    } else {
        const int sign = (remaining > 0) ? 1 : -1;
        fraction       = (((sign > 0) == currentDiagonalA) ? fractionMax : fractionMin) * config.actuateFraction;
    }
    motion.moveToFraction(ServoId::Translation, fraction, config.scaled(config.actuateMs));
}
