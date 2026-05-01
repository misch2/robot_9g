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
    //                                  min, max, rest,  direction
    {ServoId::FrontLeft,   "FrontLeft",   PIN_SERVO_1, 0,  90,  0.0f,   +1}, // lifts up
    {ServoId::FrontRight,  "FrontRight",  PIN_SERVO_2, 90, 180, 180.0f, -1}, // rests at upper limit, lifts down
    {ServoId::RearLeft,    "RearLeft",    PIN_SERVO_3, 0,  90,  0.0f,   +1}, // lifts up
    {ServoId::RearRight,   "RearRight",   PIN_SERVO_4, 90, 180, 180.0f, -1}, // rests at upper limit, lifts down
    {ServoId::Rotation,    "Rotation",    PIN_SERVO_5, 45, 135, 90.0f,  -1}, // primary = left
    {ServoId::Translation, "Translation", PIN_SERVO_6, 25, 155, 90.0f,  +1}, // primary = forward
};

static_assert(sizeof(kServos) / sizeof(kServos[0]) == static_cast<size_t>(ServoId::_Count),
              "kServos must have one entry per ServoId, in enum order");
