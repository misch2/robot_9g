#include "servo_motion.h"

namespace {
float applyEasing(Easing e, float t) {
    switch (e) {
        case Easing::Linear:    return t;
        case Easing::EaseIn:    return t * t;
        case Easing::EaseOut:   { float u = 1.0f - t; return 1.0f - u * u; }
        case Easing::EaseInOut: return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }
    return t;
}
}  // namespace

void ServoMotion::moveTo(ServoId id, float angle, uint32_t durationMs, Easing easing) {
    size_t i = static_cast<size_t>(id);
    if (i >= kCount) return;

    if (durationMs == 0) {
        manager.setAngle(id, angle);
        motions[i].active = false;
        return;
    }

    Motion& m  = motions[i];
    m.startAngle  = manager.getCurrentAngle(id);
    m.targetAngle = angle;
    m.startMs     = millis();
    m.durationMs  = durationMs;
    m.easing      = easing;
    m.active      = true;
}

void ServoMotion::update() {
    uint32_t now = millis();
    for (size_t i = 0; i < kCount; i++) {
        Motion& m = motions[i];
        if (!m.active) continue;

        ServoId id = static_cast<ServoId>(i);
        uint32_t elapsed = now - m.startMs;
        if (elapsed >= m.durationMs) {
            manager.setAngle(id, m.targetAngle);
            m.active = false;
            continue;
        }

        float t     = (float)elapsed / (float)m.durationMs;
        float k     = applyEasing(m.easing, t);
        float angle = m.startAngle + (m.targetAngle - m.startAngle) * k;
        manager.setAngle(id, angle);
    }
}

bool ServoMotion::isIdle() const {
    for (size_t i = 0; i < kCount; i++) {
        if (motions[i].active) return false;
    }
    return true;
}

bool ServoMotion::isIdle(ServoId id) const {
    size_t i = static_cast<size_t>(id);
    if (i >= kCount) return true;
    return !motions[i].active;
}
