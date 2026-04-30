#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>
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
    bool attached;       // whether the servo is currently attached

    ServoSettings(int pinNumber, int minPulseLength, int maxPulseLength, int minAngle, int maxAngle, int neutralAngle)
        : pinNumber(pinNumber), minPulseLength(minPulseLength), maxPulseLength(maxPulseLength), minAngle(minAngle), maxAngle(maxAngle), neutralAngle(neutralAngle), currentAngle((float)neutralAngle), attached(false) {}
};

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
    std::vector<Servo> servos;
    std::vector<ServoSettings> settings;
    void attachServo(size_t index);
    void detachServo(size_t index);
    float clampAngle(size_t index, float angle) const;
    int angleToMicroseconds(size_t index, float angle) const;
};
