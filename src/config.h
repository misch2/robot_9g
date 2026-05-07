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
    HeadPan,
    HeadTilt,
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
    float minAngle, maxAngle;
    float restAngle;
    int8_t primaryDirection;
};

constexpr float legAngleRange = 90.0f;  // how far each leg servo can lift from its rest position toward the primary limit, in degrees. Tune to your geometry and HW limits.

constexpr float frontLeftMinAngle  = 10.0f;  // higher value = raise foot less
constexpr float rearLeftMinAngle   = frontLeftMinAngle;
constexpr float frontRightMinAngle = 92.0f;  // higher value = raise foot more
constexpr float rearRightMinAngle  = frontRightMinAngle;

// Hardware-specific servo configuration. Tune these to your geometry and HW limits.
constexpr ServoSpec kServos[] = {
    //
    {ServoId::FrontLeft,   "FrontLeft",   PIN_SERVO_1, frontLeftMinAngle,  frontLeftMinAngle + legAngleRange,  frontLeftMinAngle + legAngleRange, -1}, // rest = standing (extended); primary = foot up (toward 0). Extra 5° beyond 90 on the rest side accommodates full standing extension.
    {ServoId::FrontRight,  "FrontRight",  PIN_SERVO_2, frontRightMinAngle, frontRightMinAngle + legAngleRange, frontRightMinAngle,                +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::RearLeft,    "RearLeft",    PIN_SERVO_3, rearLeftMinAngle,   rearLeftMinAngle + legAngleRange,   rearLeftMinAngle + legAngleRange,  -1}, // rest = standing (extended); primary = foot up (toward 0)
    {ServoId::RearRight,   "RearRight",   PIN_SERVO_4, rearRightMinAngle,  rearRightMinAngle + legAngleRange,  rearRightMinAngle,                 +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::Rotation,    "Rotation",    PIN_SERVO_5, 10.0f,              100.0f,                             100.0f,                            -1}, // primary = rotate RearLeft + FrontRight to the right and RearRight + FrontLeft to the left
    {ServoId::Translation, "Translation", PIN_SERVO_6, 10.0f,              150.0f,                             80.0f,                             +1}, // primary = move RearLeft + FrontRight forward and RearRight + FrontLeft backward
    {ServoId::HeadPan,     "HeadPan",     PIN_SERVO_7, 45.0f,              135.0f,                             90.0f,                             +1}, // primary = turn head left and right
    {ServoId::HeadTilt,    "HeadTilt",    PIN_SERVO_8, 45.0f,              135.0f,                             90.0f,                             +1}, // primary = tilt head up and down
};

static_assert(sizeof(kServos) / sizeof(kServos[0]) == static_cast<size_t>(ServoId::_Count),
              "kServos must have one entry per ServoId, in enum order");
