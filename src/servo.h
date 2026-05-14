#pragma once

#include <Arduino.h>

// PCA9685 has 16 PWM channels.
inline constexpr size_t kMaxServos = 16;

// One-time backend init: brings up Wire and the shared Adafruit_PWMServoDriver.
// Must be called before any Servo::attach().
void servoBackendBegin();

// Drives a single hobby servo through a PCA9685 PWM channel (0..15). Not RAII —
// attach()/detach() are explicit so instances can live in a std::vector
// (ServoManager owns the lifecycle).
class Servo {
public:
    Servo(uint8_t channel, int minPulseLength, int maxPulseLength,
          int minAngle, int maxAngle, int neutralAngle);

    void attach();
    void detach();
    void setAngle(float angle);

    float getCurrentAngle() const { return currentAngle; }
    int getNeutralAngle() const { return neutralAngle; }
    uint8_t getChannel() const { return channel; }

private:
    float clampAngle(float angle) const;
    int angleToMicroseconds(float angle) const;
    void writeMicroseconds(int microseconds);

    uint8_t channel;
    int minPulseLength;
    int maxPulseLength;
    int minAngle;
    int maxAngle;
    int neutralAngle;
    float currentAngle;
    bool attached;
    int lastPulseUs;
};
