#include "pose_mover.h"

namespace {
constexpr ServoId kAllLegs[4] = {ServoId::FrontLeft, ServoId::FrontRight,
                                 ServoId::RearLeft, ServoId::RearRight};
}  // namespace

PoseMover::PoseMover(ServoMotion& m) : motion(m) {}

void PoseMover::start(Pose pose, const RobotConfig& config) {
    switch (pose) {
        case Pose::Stand:
            for (ServoId leg : kAllLegs) motion.moveToFraction(leg, fractionBalanced, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::Translation, fractionBalanced, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::Rotation, fractionBalanced, config.scaled(config.poseMs));
            break;
        case Pose::Crouch:
            // Crouch deliberately leaves Translation/Rotation alone.
            for (ServoId leg : kAllLegs) motion.moveToFraction(leg, config.crouchFraction, config.scaled(config.poseMs));
            break;
        case Pose::Sit:
            // Sit = back on the ground, front raised. Only the rear legs
            // lift the body; the fronts stay extended. Body actuators
            // recenter.
            motion.moveToFraction(ServoId::RearLeft, fractionMax, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::RearRight, fractionMax, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::FrontLeft, fractionBalanced, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::FrontRight, fractionBalanced, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::Translation, fractionBalanced, config.scaled(config.poseMs));
            motion.moveToFraction(ServoId::Rotation, fractionBalanced, config.scaled(config.poseMs));
            break;
    }
    active = true;
}

bool PoseMover::update(uint32_t /*now*/) {
    if (!active) return true;
    if (!motion.isIdle()) return false;
    active = false;
    return true;
}
