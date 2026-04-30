#include "servo_manager.h"

ServoManager::~ServoManager() {
    for (size_t i = 0; i < settings.size(); i++) {
        detachServo(i);
    }
}

void ServoManager::addServo(ServoId id, int pinNumber, int minPulseLength, int maxPulseLength,
                            int minAngle, int maxAngle, int neutralAngle) {
    size_t index = static_cast<size_t>(id);
    // Enforce that registration order matches the ServoId enum order, so the
    // enum value can be used directly as the array index.
    if (index != settings.size()) {
        Serial.printf("ServoManager: addServo for id %u out of order (expected %u)\n",
                      (unsigned)index, (unsigned)settings.size());
        return;
    }
    settings.push_back(ServoSettings(pinNumber, minPulseLength, maxPulseLength, minAngle, maxAngle, neutralAngle));
}

void ServoManager::begin() {
    for (size_t i = 0; i < settings.size(); i++) {
        attachServo(i);
        servos[i].write((int)clampAngle(i, settings[i].neutralAngle));
    }
}

void ServoManager::attachServo(size_t index) {
    if (index >= settings.size()) return;
    if (!settings[index].attached) {
        servos[index].attach(settings[index].pinNumber, settings[index].minPulseLength, settings[index].maxPulseLength);
        settings[index].attached = true;
    }
}

void ServoManager::detachServo(size_t index) {
    if (index >= settings.size()) return;
    if (settings[index].attached) {
        servos[index].detach();
        settings[index].attached = false;
    }
}

float ServoManager::clampAngle(size_t index, float angle) {
    if (index >= settings.size()) return 0.0f;
    if (angle < settings[index].minAngle) return settings[index].minAngle;
    if (angle > settings[index].maxAngle) return settings[index].maxAngle;
    return angle;
}

void ServoManager::setAngle(ServoId id, float angle) {
    size_t index = static_cast<size_t>(id);
    if (index >= settings.size()) return;
    attachServo(index);
    float clamped = clampAngle(index, angle);
    settings[index].currentAngle = clamped;
    servos[index].write((int)clamped);
}

float ServoManager::getCurrentAngle(ServoId id) const {
    size_t index = static_cast<size_t>(id);
    if (index >= settings.size()) return 0.0f;
    return settings[index].currentAngle;
}
