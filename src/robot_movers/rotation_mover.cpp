#include "rotation_mover.h"

namespace {
constexpr ServoId kDiagA[2] = {ServoId::RearLeft, ServoId::FrontRight};
constexpr ServoId kDiagB[2] = {ServoId::FrontLeft, ServoId::RearRight};
}  // namespace

RotationMover::RotationMover(ServoMotion& m, const RobotConfig& c) : motion(m), config(c) {}

void RotationMover::start(int signedFullSteps) {
    if (signedFullSteps == 0) return;
    halfStepsRemaining = (signedFullSteps > 0 ? signedFullSteps : -signedFullSteps) * 2;
    // sign>0 lifts diagA first; sign<0 lifts diagB first. Servo always
    // starts a job at 0 (and ends at 0 since each job is an even number
    // of half-steps), so no cross-job state to consult.
    currentDiagonalA = signedFullSteps > 0;
    servoAtExtreme   = false;
    active           = true;
    directionSign    = signedFullSteps > 0 ? +1 : -1;
    if (config.headFollowsRotation) {
        motion.moveToFraction(ServoId::HeadRotation,
                              directionSign * config.headFollowFraction,
                              config.scaled(config.headFollowMs));
    }
    beginHalfStep(millis());
}

void RotationMover::beginHalfStep(uint32_t now) {
    issueLift();

    // The lift/drop pair for this half-step targets one diagonal; the next
    // half-step lifts the *other* diagonal, so its lift can overlap this
    // step's drop without conflict. We therefore end this half-step as soon
    // as the body has settled (bodySpan) and let the leg drop continue into
    // the next half-step's lift. Both diagonals are briefly airborne mid-
    // transition — that's accepted; the robot just touches down.
    //
    // Exception: the *last* half-step of the job extends to whichever of
    // legSpan / bodySpan is longer, so the job ends with all legs grounded.
    // Drop scheduling stays anchored to the legs-land point (legSpan from
    // start) so drop timing relative to body settle is preserved.
    const uint32_t legSpan  = config.scaled(config.legLiftMs) + config.scaled(config.legDropMs);
    const uint32_t bodySpan = config.scaled(config.bodyLeadInMs) + config.scaled(config.actuateMs) + config.scaled(config.bodySettleMs);
    const uint32_t landing  = legSpan > bodySpan ? legSpan : bodySpan;
    const bool isLast       = (halfStepsRemaining == 1);

    actuateAtMs   = now + config.scaled(config.bodyLeadInMs);
    halfStepEndMs = now + (isLast ? landing : bodySpan);
    dropAtMs      = now + landing - config.scaled(config.legDropMs);
    actuateIssued = false;
    dropIssued    = false;
}

bool RotationMover::update(uint32_t now) {
    if (!active) return true;

    if (!actuateIssued && (int32_t)(now - actuateAtMs) >= 0) {
        issueActuate();
        actuateIssued = true;
    }
    if (!dropIssued && (int32_t)(now - dropAtMs) >= 0) {
        issueDrop();
        dropIssued = true;
    }
    if ((int32_t)(now - halfStepEndMs) < 0) return false;

    // Half-step complete: the servo is now at the position issueActuate
    // drove it to, and the next half-step lifts the other diagonal.
    servoAtExtreme   = !servoAtExtreme;
    currentDiagonalA = !currentDiagonalA;
    halfStepsRemaining--;

    if (halfStepsRemaining == 0) {
        active = false;
        if (config.headFollowsRotation) {
            motion.moveToFraction(ServoId::HeadRotation, fractionBalanced,
                                  config.scaled(config.headFollowMs));
        }
        return true;
    }
    beginHalfStep(now);
    return false;
}

void RotationMover::issueLift() {
    const ServoId* pair  = currentDiagonalA ? kDiagA : kDiagB;
    const uint32_t durMs = config.scaled(config.legLiftMs);
    motion.moveToFraction(pair[0], config.liftFraction, durMs);
    motion.moveToFraction(pair[1], config.liftFraction, durMs);
}

void RotationMover::issueDrop() {
    const ServoId* pair  = currentDiagonalA ? kDiagA : kDiagB;
    const uint32_t durMs = config.scaled(config.legDropMs);
    motion.moveToFraction(pair[0], fractionBalanced, durMs);
    motion.moveToFraction(pair[1], fractionBalanced, durMs);
}

void RotationMover::issueActuate() {
    // The rotation servo is mechanically one-sided: it only moves between 0
    // and +max. Each half-step toggles it to the opposite of its current
    // position; direction is set by which diagonal is lifted (see start()).
    const float fraction = servoAtExtreme ? fractionBalanced : fractionMax * config.actuateFraction;
    motion.moveToFraction(ServoId::Rotation, fraction, config.scaled(config.actuateMs));
}
