#include "servo_manager.h"

ServoManager::ServoManager() {
    servos.reserve(kCount);
}

ServoManager::~ServoManager() {
    for (auto& s : servos) {
        s.detach();
    }
}

void ServoManager::addServo(ServoId id, int pinNumber, int minPulseLength, int maxPulseLength,
                            int minAngle, int maxAngle, int neutralAngle) {
    size_t index = static_cast<size_t>(id);
    if (index >= kCount) {
        Serial.printf("ServoManager: addServo id %u exceeds kCount %u\n",
                      (unsigned)index, (unsigned)kCount);
        return;
    }
    // Enforce that registration order matches the ServoId enum order, so the
    // enum value can be used directly as the array/channel index.
    if (index != servos.size()) {
        Serial.printf("ServoManager: addServo for id %u out of order (expected %u)\n",
                      (unsigned)index, (unsigned)servos.size());
        return;
    }
    servos.emplace_back((uint8_t)index, pinNumber, minPulseLength, maxPulseLength,
                        minAngle, maxAngle, neutralAngle);
    Serial.printf("ServoManager: addServo id=%u pin=%d pulse=[%d..%d]us angles=[%d..%d] neutral=%d\n",
                  (unsigned)index, pinNumber, minPulseLength, maxPulseLength, minAngle, maxAngle, neutralAngle);
}

void ServoManager::begin() {
    for (auto& s : servos) {
        s.attach();
        s.setAngle(s.getNeutralAngle());
    }
}

void ServoManager::setAngle(ServoId id, float angle) {
    size_t index = static_cast<size_t>(id);
    if (index >= servos.size()) return;
    servos[index].setAngle(angle);
}

float ServoManager::getCurrentAngle(ServoId id) const {
    size_t index = static_cast<size_t>(id);
    if (index >= servos.size()) return 0.0f;
    return servos[index].getCurrentAngle();
}
