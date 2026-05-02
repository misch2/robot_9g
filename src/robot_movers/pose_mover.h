#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../servo_motion.h"
#include "robot_config.h"

// Drive every relevant servo to a static target pose; done when motion
// finishes. Pose layouts live in this class because they are tiny and
// always come together with their dispatch.
class PoseMover {
public:
    enum class Pose : uint8_t { Stand,
                                Crouch,
                                Sit };

    explicit PoseMover(ServoMotion& motion);

    void start(Pose pose, const RobotConfig& config);
    bool update(uint32_t now);

private:
    ServoMotion& motion;
    bool active = false;
};
