#pragma once

#include <Arduino.h>

// Backend selection — set via -D in platformio.ini. Default to LEDC if neither
// is defined so a bare `pio run` keeps working.
#if !defined(SERVO_BACKEND_LEDC) && !defined(SERVO_BACKEND_PCA9685)
#define SERVO_BACKEND_LEDC 1
#endif

#if defined(SERVO_BACKEND_PCA9685)
inline constexpr size_t kMaxServos = 16;  // PCA9685 has 16 PWM channels
#else
inline constexpr size_t kMaxServos = 8;   // ESP32-S3 LEDC channel limit
#endif

// One-time backend init. LEDC: no-op. PCA9685: brings up Wire and the shared
// Adafruit_PWMServoDriver. Must be called before any Servo::attach().
void servoBackendBegin();

// Drives a single hobby servo. Channel meaning depends on the backend
// (LEDC channel 0..7 or PCA9685 channel 0..15). Not RAII — attach()/detach()
// are explicit so instances can live in a std::vector (ServoManager owns the
// lifecycle).
class Servo {
public:
    Servo(uint8_t channel, int pinNumber, int minPulseLength, int maxPulseLength,
          int minAngle, int maxAngle, int neutralAngle);

    void attach();
    void detach();
    void setAngle(float angle);

    float getCurrentAngle() const { return currentAngle; }
    int getNeutralAngle() const { return neutralAngle; }
    int getPinNumber() const { return pinNumber; }
    uint8_t getChannel() const { return channel; }

private:
    float clampAngle(float angle) const;
    int angleToMicroseconds(float angle) const;
    void writeMicroseconds(int microseconds);

    uint8_t channel;
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
