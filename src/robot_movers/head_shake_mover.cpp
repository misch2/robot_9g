#include "head_shake_mover.h"

HeadShakeMover::HeadShakeMover(ServoMotion& m, const RobotConfig& c) : motion(m), config(c) {}

void HeadShakeMover::start(int shakes) {
    if (shakes <= 0) return;
    remaining  = shakes;
    toPositive = true;
    centring   = false;
    active     = true;
    issueNext(millis());
}

void HeadShakeMover::issueNext(uint32_t now) {
    const uint32_t durMs = config.scaled(config.headShakeMs);
    if (remaining > 0) {
        const float target = (toPositive ? +1.0f : -1.0f) * config.headShakeFraction;
        motion.moveToFraction(ServoId::HeadPan, target, durMs);
        toPositive = !toPositive;
        remaining--;
        phaseEnd = now + durMs;
    } else {
        motion.moveToFraction(ServoId::HeadPan, fractionBalanced, durMs);
        centring = true;
        phaseEnd = now + durMs;
    }
}

bool HeadShakeMover::update(uint32_t now) {
    if (!active) return true;
    if ((int32_t)(now - phaseEnd) < 0) return false;
    if (centring) {
        active   = false;
        centring = false;
        return true;
    }
    issueNext(now);
    return false;
}
