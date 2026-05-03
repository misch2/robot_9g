#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Oscillates the head left/right N times and returns it to centre. One shake
// is one transition from one side to the other; a typical "no" gesture is
// 3-4 shakes.
class HeadShakeMover {
public:
    HeadShakeMover(ServoMotion& motion, const RobotConfig& config);

    void start(int shakes);
    bool update(uint32_t now);

private:
    void issueNext(uint32_t now);

    ServoMotion& motion;
    const RobotConfig& config;

    int remaining     = 0;
    bool toPositive   = true;
    bool active       = false;
    bool centring     = false;
    uint32_t phaseEnd = 0;
};
