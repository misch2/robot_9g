#include "dance_mover.h"

namespace {
// Clockwise viewed from above with the robot facing forward.
constexpr ServoId kClockwiseLegs[4] = {ServoId::FrontLeft, ServoId::FrontRight,
                                       ServoId::RearRight, ServoId::RearLeft};
}  // namespace

DanceMover::DanceMover(ServoMotion& m, const RobotConfig& c) : motion(m), config(c) {}

void DanceMover::start(int legSteps) {
    if (legSteps <= 0) return;
    remaining = legSteps;
    legIdx    = 0;
    active    = true;
    beginLegStep(millis());
}

void DanceMover::beginLegStep(uint32_t now) {
    const ServoId leg = kClockwiseLegs[legIdx & 3];
    motion.moveToFraction(leg, config.liftFraction, config.scaled(config.legLiftMs));
    dropAtMs   = now + config.scaled(config.legLiftMs) + config.scaled(config.danceHoldMs);
    stepEndMs  = dropAtMs + config.scaled(config.legDropMs);
    dropIssued = false;
}

bool DanceMover::update(uint32_t now) {
    if (!active) return true;

    const ServoId leg = kClockwiseLegs[legIdx & 3];
    if (!dropIssued && (int32_t)(now - dropAtMs) >= 0) {
        motion.moveToFraction(leg, fractionBalanced, config.scaled(config.legDropMs));
        dropIssued = true;
    }
    if ((int32_t)(now - stepEndMs) < 0) return false;

    legIdx = (legIdx + 1) & 3;
    if (remaining > 0) remaining--;
    if (remaining == 0) {
        active = false;
        return true;
    }
    beginLegStep(now);
    return false;
}
