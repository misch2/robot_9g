#include "robot_motion.h"
#include <math.h>

RobotMotion::RobotMotion(ServoMotion& m)
    : motion(m), halfStepMover(m, config), rotationMover(m, config), danceMover(m, config), poseMover(m), headShakeMover(m, config), headNodMover(m, config), headLookAroundMover(m, config) {}

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
    int fullSteps = (int)lroundf(degrees / config.degreesPerRotation);
    if (fullSteps == 0) return;
    enqueue({Action::Rotate, fullSteps});
}

void RobotMotion::sit() { enqueue({Action::Sit}); }
void RobotMotion::crouch() { enqueue({Action::Crouch}); }
void RobotMotion::stand() { enqueue({Action::Stand}); }

void RobotMotion::dance(int rotations) {
    if (rotations <= 0) return;
    enqueue({Action::Dance, rotations * 4});
}

void RobotMotion::shakeNo(int shakes) {
    if (shakes <= 0) return;
    enqueue({Action::HeadShake, shakes});
}

void RobotMotion::nodYes(int nods) {
    if (nods <= 0) return;
    enqueue({Action::HeadNod, nods});
}

void RobotMotion::lookAround(int cycles, HeadLookAroundMover::Pattern pattern) {
    if (cycles <= 0) return;
    Action a = (pattern == HeadLookAroundMover::Pattern::Figure8)
                   ? Action::HeadLookAroundFigure8
                   : Action::HeadLookAroundCircular;
    enqueue({a, cycles});
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
        case Action::HeadShake:
            headShakeMover.start(j.param);
            active = Active::HeadShake;
            break;
        case Action::HeadNod:
            headNodMover.start(j.param);
            active = Active::HeadNod;
            break;
        case Action::HeadLookAroundCircular:
            headLookAroundMover.start(j.param, HeadLookAroundMover::Pattern::Circular);
            active = Active::HeadLookAround;
            break;
        case Action::HeadLookAroundFigure8:
            headLookAroundMover.start(j.param, HeadLookAroundMover::Pattern::Figure8);
            active = Active::HeadLookAround;
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
        case Active::HeadShake:
            done = headShakeMover.update(now);
            break;
        case Active::HeadNod:
            done = headNodMover.update(now);
            break;
        case Active::HeadLookAround:
            done = headLookAroundMover.update(now);
            break;
        case Active::None:
            done = true;
            break;
    }
    if (!done) return;

    // After a Walk job finishes its regular half-steps the body actuator
    // is left at one extreme. If the next queued job is another Walk in
    // the same direction we dispatch it directly so the gait continues
    // uninterrupted; otherwise we run a single recenter half-step before
    // whatever comes next.
    if (active == Active::HalfStep && halfStepMover.needsRecenter()) {
        if (!queueEmpty()) {
            const Job& next = queue[head];
            if (next.action == Action::Walk &&
                ((next.param > 0) == (halfStepMover.direction() > 0))) {
                Job j = next;
                head  = (head + 1) % kMaxJobs;
                startJob(j);
                return;
            }
        }
        halfStepMover.startRecenter();
        return;
    }

    active = Active::None;
    if (queueEmpty()) return;

    Job j = queue[head];
    head  = (head + 1) % kMaxJobs;
    startJob(j);
}
