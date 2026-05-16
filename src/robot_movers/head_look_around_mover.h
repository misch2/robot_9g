#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Drives HeadPan + HeadTilt as sine waves so the head traces a continuous
// path through its full reachable cone. One cycle = one full traversal of
// the chosen pattern.
//   Circular: pan = sin(2π t/T),       tilt = sin(2π t/T + π/2)
//             → head sweeps in a circle (pan & tilt 90° out of phase).
//   Figure8:  pan = sin(2*2π t/T),     tilt = sin(2π t/T)
//             → Lissajous 2:1, pan oscillates twice per tilt cycle.
// Amplitudes ramp in/out over `headLookAroundRampMs` so the head does not
// snap at the start or stop sharply at the end. After the cycles complete,
// both servos are smoothly recentred.
class HeadLookAroundMover {
public:
    enum class Pattern : uint8_t { Circular, Figure8 };

    HeadLookAroundMover(ServoMotion& motion, const RobotConfig& config);

    void start(int cycles, Pattern pattern);
    bool update(uint32_t now);

private:
    ServoMotion& motion;
    const RobotConfig& config;

    bool active       = false;
    bool recentring   = false;
    Pattern pattern   = Pattern::Circular;
    uint32_t startMs  = 0;
    uint32_t durMs    = 0;
    uint32_t rampMs   = 0;
    uint32_t recenterEndMs = 0;
};
