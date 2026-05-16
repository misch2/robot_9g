#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Oscillates the head up/down N times via HeadTilt and returns it to centre.
// One nod is one transition from one extreme to the other; a typical "yes"
// gesture is 3-4 nods.
class HeadNodMover {
public:
    HeadNodMover(ServoMotion& motion, const RobotConfig& config);

    void start(int nods);
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
