#pragma once

#include <Arduino.h>
#include "config.h"
#include "servo_manager.h"

enum class Easing : uint8_t {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

// Drives smooth, time-based servo movements on top of ServoManager. Multiple
// servos can move concurrently; each has independent state. Call update() from
// the main loop. moveTo() on an already-moving servo retargets from the
// current interpolated angle so the motion stays smooth.
class ServoMotion {
public:
    explicit ServoMotion(ServoManager& manager) : manager(manager) {}

    void moveTo(ServoId id, float angle, uint32_t durationMs, Easing easing = Easing::EaseInOut);
    void moveToFraction(ServoId id, float fraction, uint32_t durationMs,
                        Easing easing = Easing::EaseInOut);
    void update();

    bool isIdle() const;
    bool isIdle(ServoId id) const;

private:
    static constexpr size_t kCount = static_cast<size_t>(ServoId::_Count);

    struct Motion {
        bool active = false;
        float startAngle = 0.0f;
        float targetAngle = 0.0f;
        uint32_t startMs = 0;
        uint32_t durationMs = 0;
        Easing easing = Easing::EaseInOut;
    };

    ServoManager& manager;
    Motion motions[kCount];
};
