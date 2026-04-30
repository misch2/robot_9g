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
    bool attached;       // whether the servo is currently attached

    ServoSettings(int pinNumber, int minPulseLength, int maxPulseLength, int minAngle, int maxAngle, int neutralAngle)
        : pinNumber(pinNumber), minPulseLength(minPulseLength), maxPulseLength(maxPulseLength), minAngle(minAngle), maxAngle(maxAngle), neutralAngle(neutralAngle), attached(false) {}
};

class ServoManager {
public:
    ServoManager(int activeCount) : activeCount(activeCount) {};
    ~ServoManager();
    void addServo(int pinNumber, int minPulseLength, int maxPulseLength,
                  int minAngle = 0, int maxAngle = 180, int neutralAngle = 90) {
        if (activeCount >= SERVOS_COUNT_LIMIT) return;  // Prevent overflow
        settings.push_back(ServoSettings(pinNumber, minPulseLength, maxPulseLength, minAngle, maxAngle, neutralAngle));
        activeCount++;
    }

    void begin();
    void setAngle(int index, float angle);

private:
    int activeCount = 0;  // Number of active servos (can be less than SERVO_COUNT)
    Servo servos[SERVOS_COUNT_LIMIT];
    std::vector<ServoSettings> settings;
    void attachServo(int index);
    void detachServo(int index);
    float clampAngle(int index, float angle);
};
