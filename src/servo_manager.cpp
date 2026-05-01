#include "servo_manager.h"

namespace {
constexpr uint32_t kServoFreqHz       = 50;
constexpr uint8_t kServoResolutionBit = 14;  // 16-bit @ 50Hz fails ledcSetup on arduino-esp32 v2.x
constexpr uint32_t kServoPeriodUs     = 1000000u / kServoFreqHz;    // 20000 us
constexpr uint32_t kServoMaxDuty      = 1u << kServoResolutionBit;  // 65536

uint32_t pulseToDuty(int microseconds) {
    if (microseconds < 0) microseconds = 0;
    return ((uint64_t)microseconds * kServoMaxDuty) / kServoPeriodUs;
}
}  // namespace

ServoManager::ServoManager() {
    settings.reserve(kCount);
}

ServoManager::~ServoManager() {
    for (size_t i = 0; i < settings.size(); i++) {
        detachServo(i);
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
    if (index != settings.size()) {
        Serial.printf("ServoManager: addServo for id %u out of order (expected %u)\n",
                      (unsigned)index, (unsigned)settings.size());
        return;
    }
    settings.push_back(ServoSettings(pinNumber, minPulseLength, maxPulseLength, minAngle, maxAngle, neutralAngle));
    Serial.printf("ServoManager: addServo id=%u pin=%d pulse=[%d..%d]us angles=[%d..%d] neutral=%d\n",
                  (unsigned)index, pinNumber, minPulseLength, maxPulseLength, minAngle, maxAngle, neutralAngle);
}

void ServoManager::begin() {
    for (size_t i = 0; i < settings.size(); i++) {
        attachServo(i);
        setAngle(static_cast<ServoId>(i), settings[i].neutralAngle);
    }
}

void ServoManager::attachServo(size_t index) {
    if (index >= settings.size()) return;
    if (!settings[index].attached) {
        int pin = settings[index].pinNumber;
        pinMode(pin, OUTPUT);
        double actualHz = ledcSetup((uint8_t)index, kServoFreqHz, kServoResolutionBit);
        ledcAttachPin(pin, (uint8_t)index);
        settings[index].attached = (actualHz > 0.0);
        Serial.printf("ServoManager: attach id=%u pin=%d ch=%u req=%uHz/%ubit actual=%.2fHz %s\n",
                      (unsigned)index, pin, (unsigned)index,
                      (unsigned)kServoFreqHz, (unsigned)kServoResolutionBit,
                      actualHz,
                      (actualHz > 0.0) ? "OK" : "FAILED");
    }
}

void ServoManager::detachServo(size_t index) {
    if (index >= settings.size()) return;
    if (settings[index].attached) {
        ledcDetachPin(settings[index].pinNumber);
        settings[index].attached = false;
    }
}

float ServoManager::clampAngle(size_t index, float angle) const {
    if (index >= settings.size()) return 0.0f;
    if (angle < settings[index].minAngle) return settings[index].minAngle;
    if (angle > settings[index].maxAngle) return settings[index].maxAngle;
    return angle;
}

int ServoManager::angleToMicroseconds(size_t index, float angle) const {
    // Linear map: 0..180 deg -> minPulseLength..maxPulseLength us. Done in
    // float for sub-degree precision lost when going through int angles.
    const ServoSettings& s = settings[index];
    float us               = s.minPulseLength + (angle / 180.0f) * (s.maxPulseLength - s.minPulseLength);
    return (int)lroundf(us);
}

void ServoManager::writeMicroseconds(size_t index, int microseconds) {
    ledcWrite((uint8_t)index, pulseToDuty(microseconds));
}

void ServoManager::setAngle(ServoId id, float angle) {
    size_t index = static_cast<size_t>(id);
    if (index >= settings.size()) return;
    attachServo(index);
    float clamped                = clampAngle(index, angle);
    settings[index].currentAngle = clamped;
    int us                       = angleToMicroseconds(index, clamped);
    if (us != settings[index].lastPulseUs) {
        uint32_t duty = pulseToDuty(us);
        Serial.printf("ServoManager: setAngle id=%u pin=%d angle=%.1f -> %dus (duty=%u)\n",
                      (unsigned)index, settings[index].pinNumber, clamped, us, (unsigned)duty);
        settings[index].lastPulseUs = us;
    }
    writeMicroseconds(index, us);
}

float ServoManager::getCurrentAngle(ServoId id) const {
    size_t index = static_cast<size_t>(id);
    if (index >= settings.size()) return 0.0f;
    return settings[index].currentAngle;
}
