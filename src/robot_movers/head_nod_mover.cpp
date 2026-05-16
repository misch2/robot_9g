#include "head_nod_mover.h"

HeadNodMover::HeadNodMover(ServoMotion& m, const RobotConfig& c) : motion(m), config(c) {}

void HeadNodMover::start(int nods) {
    if (nods <= 0) return;
    remaining  = nods;
    toPositive = true;
    centring   = false;
    active     = true;
    issueNext(millis());
}

void HeadNodMover::issueNext(uint32_t now) {
    const uint32_t durMs = config.scaled(config.headNodMs);
    if (remaining > 0) {
        const float target = (toPositive ? +1.0f : -1.0f) * config.headNodFraction;
        motion.moveToFraction(ServoId::HeadTilt, target, durMs);
        toPositive = !toPositive;
        remaining--;
        phaseEnd = now + durMs;
    } else {
        motion.moveToFraction(ServoId::HeadTilt, fractionBalanced, durMs);
        centring = true;
        phaseEnd = now + durMs;
    }
}

bool HeadNodMover::update(uint32_t now) {
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
