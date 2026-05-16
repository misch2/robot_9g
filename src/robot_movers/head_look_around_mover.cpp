#include "head_look_around_mover.h"
#include <math.h>

HeadLookAroundMover::HeadLookAroundMover(ServoMotion& m, const RobotConfig& c)
    : motion(m), config(c) {}

void HeadLookAroundMover::start(int cycles, Pattern p) {
    if (cycles <= 0) return;
    const uint32_t cycleMs = config.scaled(config.headLookAroundCycleMs);
    pattern    = p;
    durMs      = cycleMs * (uint32_t)cycles;
    rampMs     = config.scaled(config.headLookAroundRampMs);
    // Cap ramp at a quarter of the total so very short jobs still ramp.
    if (rampMs > durMs / 4) rampMs = durMs / 4;
    startMs    = millis();
    active     = true;
    recentring = false;
}

bool HeadLookAroundMover::update(uint32_t now) {
    if (!active) return true;

    if (recentring) {
        if ((int32_t)(now - recenterEndMs) < 0) return false;
        active     = false;
        recentring = false;
        return true;
    }

    uint32_t elapsed = now - startMs;
    if (elapsed >= durMs) {
        // Smoothly return both head servos to centre.
        const uint32_t backMs = config.scaled(config.headLookAroundRampMs);
        motion.moveToFraction(ServoId::HeadPan,  fractionBalanced, backMs);
        motion.moveToFraction(ServoId::HeadTilt, fractionBalanced, backMs);
        recenterEndMs = now + backMs;
        recentring    = true;
        return false;
    }

    const float twoPi   = 6.28318530717958647692f;
    const float cycleMs = (float)config.scaled(config.headLookAroundCycleMs);
    const float phase   = twoPi * (float)elapsed / cycleMs;

    // Cosine-shaped envelope: 0 at edges, 1 in the middle, ramping over rampMs.
    float env = 1.0f;
    if (rampMs > 0) {
        if (elapsed < rampMs) {
            float u = (float)elapsed / (float)rampMs;
            env = 0.5f * (1.0f - cosf((float)M_PI * u));
        } else if (elapsed > durMs - rampMs) {
            float u = (float)(durMs - elapsed) / (float)rampMs;
            env = 0.5f * (1.0f - cosf((float)M_PI * u));
        }
    }

    float panFrac;
    float tiltFrac;
    switch (pattern) {
        case Pattern::Circular:
            panFrac  = config.headLookAroundPanAmplitude  * env * sinf(phase);
            tiltFrac = config.headLookAroundTiltAmplitude * env * sinf(phase + (float)M_PI_2);
            break;
        case Pattern::Figure8:
        default:
            panFrac  = config.headLookAroundPanAmplitude  * env * sinf(2.0f * phase);
            tiltFrac = config.headLookAroundTiltAmplitude * env * sinf(phase);
            break;
    }

    // duration=0 writes immediately via ServoManager::setAngle without scheduling
    // a smooth motion (and without the per-call Serial log inside moveTo).
    motion.moveToFraction(ServoId::HeadPan,  panFrac,  0);
    motion.moveToFraction(ServoId::HeadTilt, tiltFrac, 0);
    return false;
}
