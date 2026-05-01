#include "servo_manager.h"
#include <math.h>

ServoManager::ServoManager() {
    servos.reserve(kCount);
}

ServoManager::~ServoManager() {
    for (auto& s : servos) {
        s.detach();
    }
}

void ServoManager::begin() {
    for (size_t i = 0; i < kCount; i++) {
        const ServoSpec& spec = kServos[i];
        servos.emplace_back((uint8_t)i, spec.pin, SERVO_MIN_PULSE, SERVO_MAX_PULSE,
                            spec.minAngle, spec.maxAngle, (int)lroundf(spec.restAngle));
        Serial.printf("ServoManager: id=%u pin=%d range=[%d..%d] rest=%.1f primary=%+d\n",
                      (unsigned)i, spec.pin, spec.minAngle, spec.maxAngle,
                      spec.restAngle, (int)spec.primaryDirection);
    }
    for (auto& s : servos) {
        s.attach();
        s.setAngle((float)s.getNeutralAngle());
    }
}

void ServoManager::setAngle(ServoId id, float angle) {
    size_t index = static_cast<size_t>(id);
    if (index >= servos.size()) return;
    servos[index].setAngle(angle);
}

void ServoManager::setFraction(ServoId id, float fraction) {
    setAngle(id, fractionToAngle(id, fraction));
}

float ServoManager::getCurrentAngle(ServoId id) const {
    size_t index = static_cast<size_t>(id);
    if (index >= servos.size()) return 0.0f;
    return servos[index].getCurrentAngle();
}

float ServoManager::fractionToAngle(ServoId id, float fraction) const {
    size_t index = static_cast<size_t>(id);
    if (index >= kCount) return 0.0f;
    const ServoSpec& spec = kServos[index];
    // +fraction goes toward primary limit; -fraction toward the opposite.
    const bool towardPositiveLimit = (fraction >= 0) == (spec.primaryDirection > 0);
    const int  limit               = towardPositiveLimit ? spec.maxAngle : spec.minAngle;
    return spec.restAngle + fabsf(fraction) * ((float)limit - spec.restAngle);
}
