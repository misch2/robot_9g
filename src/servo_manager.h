#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "servo.h"

// Manages the group of registered servos. The ServoId enum value is used as
// both the array index and the LEDC channel index, so addServo() must be
// called once per ServoId in declaration order; the project is bounded to
// 8 servos on ESP32-S3 (LEDC channel limit).
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
};
