#pragma once

#include <stdint.h>

#define SERVO_MIN_PULSE 500   // microseconds for 0 degrees
#define SERVO_MAX_PULSE 2500  // microseconds for 180 degrees

// Logical servo identifiers. The numeric value of each entry is its index in
// ServoManager's internal array (and its LEDC channel), so kServos below must
// have one entry per ServoId, listed in this exact order.
enum class ServoId : uint8_t {
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight,
    Rotation,
    Translation,
    _Count
};

// Per-servo calibration. Single source of truth for pin, hardware safety
// range, rest position (== 0% / standing / body-neutral), and which limit
// counts as "primary" — i.e. fraction +1.0 maps to maxAngle when
// primaryDirection > 0 and to minAngle when primaryDirection < 0.
// Negative fractions go toward the opposite limit (only meaningful for body
// servos that swing both ways from rest).
struct ServoSpec {
    ServoId id;
    const char* name;
    int pin;
    int minAngle, maxAngle;
    float restAngle;
    int8_t primaryDirection;
};

constexpr ServoSpec kServos[] = {
    //
    {ServoId::FrontLeft,   "FrontLeft",   PIN_SERVO_1, 0,  95,  95.0f,  -1}, // rest = standing (extended); primary = foot up (toward 0). Extra 5° beyond 90 on the rest side accommodates full standing extension.
    {ServoId::FrontRight,  "FrontRight",  PIN_SERVO_2, 90, 180, 90.0f,  +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::RearLeft,    "RearLeft",    PIN_SERVO_3, 0,  90,  90.0f,  -1}, // rest = standing (extended); primary = foot up (toward 0)
    {ServoId::RearRight,   "RearRight",   PIN_SERVO_4, 90, 180, 90.0f,  +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::Rotation,    "Rotation",    PIN_SERVO_5, 10, 100, 100.0f, -1}, // primary = rotate RearLeft + FrontRight to the right and RearRight + FrontLeft to the left
    {ServoId::Translation, "Translation", PIN_SERVO_6, 10, 170, 90.0f,  +1}, // primary = move RearLeft + FrontRight forward and RearRight + FrontLeft backward
};

static_assert(sizeof(kServos) / sizeof(kServos[0]) == static_cast<size_t>(ServoId::_Count),
              "kServos must have one entry per ServoId, in enum order");
