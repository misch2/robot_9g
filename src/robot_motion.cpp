#include "robot_motion.h"
#include <math.h>

RobotMotion::RobotMotion(ServoMotion& m)
    : motion(m), halfStepMover(m, config), rotationMover(m, config), danceMover(m, config), poseMover(m) {}

void RobotMotion::begin() { startJob({Action::Stand, 0}); }

void RobotMotion::enqueue(const Job& j) {
    if (queueFull()) {
        Serial.println("RobotMotion: job queue full, dropping command");
        return;
    }
    queue[tail] = j;
    tail        = (tail + 1) % kMaxJobs;
}

void RobotMotion::step(int fullSteps) {
    if (fullSteps == 0) return;
    enqueue({Action::Walk, fullSteps * 2});
}

void RobotMotion::rotate(float degrees) {
    int halfSteps = (int)lroundf(degrees / config.degreesPerHalfRotation);
    if (halfSteps == 0) return;
    enqueue({Action::Rotate, halfSteps});
}

void RobotMotion::sit() { enqueue({Action::Sit}); }
void RobotMotion::crouch() { enqueue({Action::Crouch}); }
void RobotMotion::stand() { enqueue({Action::Stand}); }

void RobotMotion::dance(int rotations) {
    if (rotations <= 0) return;
    enqueue({Action::Dance, rotations * 4});
}

bool RobotMotion::isIdle() const {
    return active == Active::None && queueEmpty();
}

void RobotMotion::startJob(const Job& j) {
    switch (j.action) {
        case Action::Walk:
            halfStepMover.start(j.param);
            active = Active::HalfStep;
            break;
        case Action::Rotate:
            rotationMover.start(j.param);
            active = Active::Rotation;
            break;
        case Action::Dance:
            danceMover.start(j.param);
            active = Active::Dance;
            break;
        case Action::Stand:
            poseMover.start(PoseMover::Pose::Stand, config);
            active = Active::Pose;
            break;
        case Action::Crouch:
            poseMover.start(PoseMover::Pose::Crouch, config);
            active = Active::Pose;
            break;
        case Action::Sit:
            poseMover.start(PoseMover::Pose::Sit, config);
            active = Active::Pose;
            break;
        case Action::None:
            break;
    }
}

void RobotMotion::update() {
    const uint32_t now = millis();

    bool done = true;
    switch (active) {
        case Active::HalfStep:
            done = halfStepMover.update(now);
            break;
        case Active::Rotation:
            done = rotationMover.update(now);
            break;
        case Active::Dance:
            done = danceMover.update(now);
            break;
        case Active::Pose:
            done = poseMover.update(now);
            break;
        case Active::None:
            done = true;
            break;
    }
    if (!done) return;

    active = Active::None;
    if (queueEmpty()) return;

    Job j = queue[head];
    head  = (head + 1) % kMaxJobs;
    startJob(j);
}
