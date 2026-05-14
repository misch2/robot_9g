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

bool ServoManager::begin() {
    if (!servoBackendBegin()) return false;
    for (size_t i = 0; i < kCount; i++) {
        const ServoSpec& spec = kServos[i];
        servos.emplace_back((uint8_t)i, SERVO_MIN_PULSE, SERVO_MAX_PULSE,
                            spec.minAngle, spec.maxAngle, (int)lroundf(spec.restAngle));
        Serial.printf("ServoManager: id=%u (%s) ch=%u range=[%d..%d] rest=%.1f primary=%+d\n",
                      (unsigned)i, spec.name, (unsigned)i, spec.minAngle, spec.maxAngle,
                      spec.restAngle, (int)spec.primaryDirection);
    }
    for (auto& s : servos) {
        s.attach();
        s.setAngle((float)s.getNeutralAngle());
    }
    return true;
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

/// Maps a fraction in [-1, +1] to an absolute angle for the given servo, based on its rest position and primary direction. The fraction is applied to the distance from the rest position to the respective limit. The primaryDirection determines which way is "positive" and which is "negative". Values outside [-1, +1] still get clamped by Servo::clampAngle() before the pulse is written.
//
// Example: if rest=90, min=0, max=180, primaryDirection=+1, then:
//   fraction=-1 -> angle=0 (fully toward min)
//   fraction=-0.5 -> angle=45 (halfway toward min)
//   fraction=0  -> angle=90 (rest)
//   fraction=+0.5 -> angle=135 (halfway toward max)
//   fraction=+1 -> angle=180 (fully toward max)
//
// Example for asymmetric limits: if rest=10, min=0, max=90, primaryDirection=-1, then:
//   fraction=-1 -> angle=90 (fully toward max, due to negative primaryDirection)
//   fraction=-0.5 -> angle=50 (halfway toward max)
//   fraction=0  -> angle=10 (rest)
//   fraction=+0.5 -> angle=5 (halfway toward min)
//   fraction=+1 -> angle=0 (fully toward min, due to negative primaryDirection)
//
float ServoManager::fractionToAngle(ServoId id, float fraction) const {
    size_t index = static_cast<size_t>(id);
    if (index >= kCount) return 0.0f;
    const ServoSpec& spec = kServos[index];
    // +fraction goes toward primary limit; -fraction toward the opposite.
    const bool towardPositiveLimit = (fraction >= 0) == (spec.primaryDirection > 0);
    const int limit                = towardPositiveLimit ? spec.maxAngle : spec.minAngle;
    return spec.restAngle + fabsf(fraction) * ((float)limit - spec.restAngle);
}
