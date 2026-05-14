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
    Rotation,
    Translation,
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

constexpr float legAngleRange = 90.0f;  // how far each leg servo can lift from its rest position toward the primary limit, in degrees. Tune to your geometry and HW limits.

// Left side: fully raised foot = lowest angle
constexpr float frontLeftMinAngle = 7.0f;
constexpr float rearLeftMinAngle  = 0.0f;
// Right side: fully raised foot = highest angle
constexpr float frontRightMinAngle = 180.0f;
constexpr float rearRightMinAngle  = 180.0f;

// Hardware-specific servo configuration. Tune these to your geometry and HW limits.
constexpr ServoSpec kServos[] = {
    //
    {ServoId::FrontLeft,   "FrontLeft",   frontLeftMinAngle,  frontLeftMinAngle + legAngleRange,  frontLeftMinAngle + legAngleRange, -1}, // rest = standing (extended); primary = foot up (toward 0). Extra 5° beyond 90 on the rest side accommodates full standing extension.
    {ServoId::FrontRight,  "FrontRight",  frontRightMinAngle, frontRightMinAngle + legAngleRange, frontRightMinAngle,                +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::RearLeft,    "RearLeft",    rearLeftMinAngle,   rearLeftMinAngle + legAngleRange,   rearLeftMinAngle + legAngleRange,  -1}, // rest = standing (extended); primary = foot up (toward 0)
    {ServoId::RearRight,   "RearRight",   rearRightMinAngle,  rearRightMinAngle + legAngleRange,  rearRightMinAngle,                 +1}, // rest = standing (extended); primary = foot up (toward 180)
    {ServoId::Rotation,    "Rotation",    25.0f,              100.0f,                             100.0f,                            -1}, // primary = rotate RearLeft + FrontRight to the right and RearRight + FrontLeft to the left
    {ServoId::Translation, "Translation", 0.0f,               175.0f,                             67.0f,                             +1}, // primary = move RearLeft + FrontRight forward and RearRight + FrontLeft backward

    // can't allow more, it would collide with electronics (step down converter)
    {ServoId::HeadPan,     "HeadPan",     40.0f,              130.0f,                             90.0f,                             +1}, // rest = look forward, primary = turn head left
    {ServoId::HeadTilt,    "HeadTilt",    45.0f,              145.0f,                             85.0f,                             +1}, // rest = look forward, primary = tilt head up
};

static_assert(sizeof(kServos) / sizeof(kServos[0]) == static_cast<size_t>(ServoId::_Count),
              "kServos must have one entry per ServoId, in enum order");
