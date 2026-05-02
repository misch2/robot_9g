#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Lift each leg in clockwise order, hold, drop. One full rotation = 4 legs.
class DanceMover {
public:
    DanceMover(ServoMotion& motion, const RobotConfig& config);

    void start(int legSteps);
    bool update(uint32_t now);

private:
    void beginLegStep(uint32_t now);

    ServoMotion& motion;
    const RobotConfig& config;

    int remaining      = 0;
    uint8_t legIdx     = 0;
    bool active        = false;
    uint32_t dropAtMs  = 0;
    uint32_t stepEndMs = 0;
    bool dropIssued    = false;
};
