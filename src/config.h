#pragma once

#include <stdint.h>

#define SERVO_MIN_PULSE 500   // microseconds for 0 degrees
#define SERVO_MAX_PULSE 2500  // microseconds for 180 degrees

// Logical servo identifiers. The numeric value of each entry is its index in
// ServoManager's internal array (and its PCA9685 channel), so kServos below
// must have one entry per ServoId, listed in this exact order.
enum class ServoId : uint8_t {
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight,
    Translation,
    Rotation,
    HeadPan,
    HeadTilt,
    _Count
};

// Per-servo calibration. Single source of truth for hardware safety range,
// rest position (== 0% / standing / body-neutral), and which limit counts as
// "primary" — i.e. fraction +1.0 maps to maxAngle when primaryDirection > 0
// and to minAngle when primaryDirection < 0. Negative fractions go toward the
// opposite limit (only meaningful for body servos that swing both ways from
// rest).
struct ServoSpec {
    ServoId id;
    const char* name;
    float minAngle, maxAngle;
    float restAngle;
    int8_t primaryDirection;
};

// Hardware-specific servo configuration. Tune these to your geometry and HW limits.
constexpr ServoSpec kServos[] = {
    // id, name,          minAngle, maxAngle, restAngle, primaryDirection

    // FrontLeft: 0 (fully bent) - 110 (fully straight)
    // FrontRight: 70 (fully straight) - 180 (fully bent)
    // RearLeft: 0 (fully bent) - 100 (fully straight)
    // RearRight: 70 (fully straight) - 180 (fully bent)
    {ServoId::FrontLeft,   "FrontLeft",   5.0f,  110.0f, 110.0f, -1}, // rest = standing (extended); primary = foot up (toward 0)
    {ServoId::FrontRight,  "FrontRight",  60.0f, 170.0f, 60.0f,  +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::RearLeft,    "RearLeft",    5.0f,  100.0f, 100.0f, -1}, // rest = standing (extended); primary = foot up (toward 0)
    {ServoId::RearRight,   "RearRight",   55.0f, 165.0f, 55.0f,  +1}, // rest = standing (extended); primary = foot up (toward 180)

    // Slider: 0 (fully retracted) - 170 (fully extended), 70 is a neutral
    // Rotator: 100 (neutral) - 10 (fully rotated)
    {ServoId::Translation, "Translation", 0.0f,  170.0f, 70.0f,  +1}, // primary = move RearLeft + FrontRight forward and RearRight + FrontLeft backward
    {ServoId::Rotation,    "Rotation",    10.0f, 100.0f, 100.0f, -1}, // primary = rotate RearLeft + FrontRight to the right and RearRight + FrontLeft to the left

    // can't allow more, it would collide with electronics (step down converter)
    {ServoId::HeadPan,     "HeadPan",     40.0f, 130.0f, 90.0f,  +1}, // rest = look forward, primary = turn head left
    {ServoId::HeadTilt,    "HeadTilt",    60.0f, 155.0f, 95.0f,  +1}, // rest = look forward, primary = tilt head up
};

static_assert(sizeof(kServos) / sizeof(kServos[0]) == static_cast<size_t>(ServoId::_Count),
              "kServos must have one entry per ServoId, in enum order");

constexpr bool kServosValid() {
    for (size_t i = 0; i < sizeof(kServos) / sizeof(kServos[0]); ++i) {
        const ServoSpec& s = kServos[i];
        if (!(s.minAngle < s.maxAngle)) return false;
        if (!(s.restAngle >= s.minAngle && s.restAngle <= s.maxAngle)) return false;
        if (s.primaryDirection != -1 && s.primaryDirection != +1) return false;
    }
    return true;
}
static_assert(kServosValid(),
              "kServos malformed: each entry needs minAngle < maxAngle, "
              "restAngle in [minAngle, maxAngle], primaryDirection ±1");
