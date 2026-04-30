#include "servo_manager.h"

ServoManager::~ServoManager() {
    for (int i = 0; i < activeCount; i++) {
        detachServo(i);
    }
}

void ServoManager::begin() {
    for (int i = 0; i < activeCount; i++) {
        setAngle(i, settings[i].neutralAngle);
    }
}

void ServoManager::attachServo(int index) {
    if (index < 0 || index >= activeCount) return;
    if (!settings[index].attached) {
        servos[index].attach(settings[index].pinNumber, settings[index].minPulseLength, settings[index].maxPulseLength);
        settings[index].attached = true;
    }
}

void ServoManager::detachServo(int index) {
    if (index < 0 || index >= activeCount) return;
    if (settings[index].attached) {
        servos[index].detach();
        settings[index].attached = false;
    }
}

float ServoManager::clampAngle(int index, float angle) {
    if (index < 0 || index >= activeCount) return 0.0f;
    if (angle < settings[index].minAngle) return settings[index].minAngle;
    if (angle > settings[index].maxAngle) return settings[index].maxAngle;
    return angle;
}

void ServoManager::setAngle(int index, float angle) {
    if (index < 0 || index >= activeCount) return;
    attachServo(index);
    servos[index].write((int)clampAngle(index, angle));
}
