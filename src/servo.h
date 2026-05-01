#pragma once

#include <Arduino.h>

// Drives a single hobby servo through one ESP32 LEDC channel. Not RAII —
// attach()/detach() are explicit so instances can live in a std::vector
// (ServoManager owns the lifecycle).
class Servo {
public:
    Servo(uint8_t ledcChannel, int pinNumber, int minPulseLength, int maxPulseLength,
          int minAngle, int maxAngle, int neutralAngle);

    void attach();
    void detach();
    void setAngle(float angle);

    float getCurrentAngle() const { return currentAngle; }
    int getNeutralAngle() const { return neutralAngle; }
    int getPinNumber() const { return pinNumber; }
    uint8_t getLedcChannel() const { return ledcChannel; }

private:
    float clampAngle(float angle) const;
    int angleToMicroseconds(float angle) const;
    void writeMicroseconds(int microseconds);

    uint8_t ledcChannel;
    int pinNumber;
    int minPulseLength;
    int maxPulseLength;
    int minAngle;
    int maxAngle;
    int neutralAngle;
    float currentAngle;
    bool attached;
    int lastPulseUs;
};
