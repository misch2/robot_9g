#pragma once

#include <Arduino.h>
#include <vector>
#include "config.h"
#include "servo.h"

// Owns the registered Servo instances and translates between fraction-based
// commands and concrete angles using kServos[] as the calibration source.
// ServoId doubles as the array index and the PCA9685 channel number, so the
// project is bounded to `kMaxServos` (16) from servo.h.
class ServoManager {
public:
    static constexpr size_t kCount = static_cast<size_t>(ServoId::_Count);
    static_assert(kCount <= kMaxServos,
                  "ServoId::_Count exceeds the PCA9685 channel limit (kMaxServos = 16)");

    ServoManager();
    ~ServoManager();

    // Builds and attaches every servo from kServos[], driving each to its
    // restAngle. Call once from setup() after Serial.begin(). Returns false
    // if the underlying PCA9685 backend could not be brought up (I2C probe
    // failed) so the caller can surface the failure on the display.
    bool begin();

    void setAngle(ServoId id, float angle);
    void setFraction(ServoId id, float fraction);
    float getCurrentAngle(ServoId id) const;

    // Maps a fraction in [-1, +1] to an absolute angle for the given servo.
    // +1 = restAngle moved fully toward the primary safety limit; -1 = fully
    // toward the opposite limit. Values outside [-1, +1] still get clamped by
    // Servo::clampAngle() before the pulse is written.
    float fractionToAngle(ServoId id, float fraction) const;

private:
    std::vector<Servo> servos;
};
