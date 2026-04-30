#pragma once

#include <stdint.h>

#define SERVO_MIN_PULSE 500   // microseconds for 0 degrees
#define SERVO_MAX_PULSE 2500  // microseconds for 180 degrees

// Logical servo identifiers. The numeric value of each entry is its index in
// ServoManager's internal array, so addServo() must be called in this order.
// Add/rename entries to match the robot's mechanics; keep _Count last.
enum class ServoId : uint8_t {
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight,
    Rotation,
    Translation,
    _Count
};
