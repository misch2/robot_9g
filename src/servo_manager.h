#pragma once

#include <Arduino.h>
#include "config.h"
#include <vector>

struct ServoSettings {
    int pinNumber;       // GPIO pin assigned to this servo
    int minPulseLength;  // microseconds
    int maxPulseLength;  // microseconds
    int minAngle;        // degrees
    int maxAngle;        // degrees
    int neutralAngle;    // degrees
    float currentAngle;  // last commanded angle (clamped, pre-int)
    bool attached;       // whether the LEDC channel is currently attached
    int lastPulseUs;     // last microsecond pulse written, for change-detection in logs

    ServoSettings(int pinNumber, int minPulseLength, int maxPulseLength, int minAngle, int maxAngle, int neutralAngle)
        : pinNumber(pinNumber), minPulseLength(minPulseLength), maxPulseLength(maxPulseLength), minAngle(minAngle), maxAngle(maxAngle), neutralAngle(neutralAngle), currentAngle((float)neutralAngle), attached(false), lastPulseUs(-1) {}
};

// Drives hobby servos directly via the ESP32 LEDC peripheral. Each registered
// servo claims one LEDC channel; the ServoId enum value is used as the channel
// index, so the project can address up to 8 servos on ESP32-S3.
class ServoManager {
public:
    static constexpr size_t kCount = static_cast<size_t>(ServoId::_Count);

    ServoManager();
    ~ServoManager();
    void addServo(ServoId id, int pinNumber, int minPulseLength, int maxPulseLength,
                  int minAngle = 0, int maxAngle = 180, int neutralAngle = 90);
    void begin();
    void setAngle(ServoId id, float angle);
    float getCurrentAngle(ServoId id) const;

private:
    std::vector<ServoSettings> settings;
    void attachServo(size_t index);
    void detachServo(size_t index);
    float clampAngle(size_t index, float angle) const;
    int angleToMicroseconds(size_t index, float angle) const;
    void writeMicroseconds(size_t index, int microseconds);
};
