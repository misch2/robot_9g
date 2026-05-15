#pragma once

#include <Arduino.h>

// PCA9685 has 16 PWM channels.
inline constexpr size_t kMaxServos = 16;

// Absolute pulse-width safety limits. Any computed pulse outside this window
// is treated as a programming error (typically mis-configured angle limits in
// config.h) and triggers the registered fatal-error handler — never written
// to the PCA9685, because over-range pulses can mechanically jam and damage
// 9 g hobby servos.
inline constexpr int kServoHardMinPulseUs = 500;
inline constexpr int kServoHardMaxPulseUs = 2500;

// Fatal-error handler invoked when a pulse exceeds the hard safety limits.
// Kept as a callback so the servo layer stays display-agnostic; main.cpp
// registers a handler that paints the face/eyes and halts.
using ServoFatalHandler = void (*)(const char* message);
void servoSetFatalHandler(ServoFatalHandler handler);

// One-time backend init: brings up Wire and the shared Adafruit_PWMServoDriver.
// Must be called before any Servo::attach(). Returns false if the PCA9685
// does not ACK on the I2C bus — caller is expected to halt and surface the
// failure (e.g. on the face display).
bool servoBackendBegin();

// Drives a single hobby servo through a PCA9685 PWM channel (0..15). Not RAII —
// attach()/detach() are explicit so instances can live in a std::vector
// (ServoManager owns the lifecycle).
class Servo {
public:
    Servo(uint8_t channel, const char* name,
          int minPulseLength, int maxPulseLength,
          int minAngle, int maxAngle, int neutralAngle);

    void attach();
    void detach();
    void setAngle(float angle);

    float getCurrentAngle() const { return currentAngle; }
    int getNeutralAngle() const { return neutralAngle; }
    uint8_t getChannel() const { return channel; }
    const char* getName() const { return name; }

private:
    float clampAngle(float angle) const;
    int angleToMicroseconds(float angle) const;
    void writeMicroseconds(int microseconds);

    uint8_t channel;
    const char* name;
    int minPulseLength;
    int maxPulseLength;
    int minAngle;
    int maxAngle;
    int neutralAngle;
    float currentAngle;
    bool attached;
    int lastPulseUs;
};
