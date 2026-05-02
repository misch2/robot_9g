#pragma once

#include <stdint.h>

// Tunables shared across all whole-robot movers. Defined at namespace scope
// so each mover can take it by reference without pulling in the orchestrator.
struct RobotConfig {
    // How far each leg lifts during the gait, as a fraction of its available
    // range from rest toward the primary limit. 1.0 would lift to the safety
    // limit; the gait usually wants less than that.
    float liftFraction = 0.67f;

    // Crouch pose: how far each leg bends in its lift direction. 1.0 =
    // pinned at the safety limit (the most extreme crouch the HW allows).
    float crouchFraction = 1.0f;

    // Body servo amplitude during a half-step actuation. Translation goes
    // ±actuateFraction to step forward/backward; Rotation does the same for
    // left/right.
    float actuateFraction = 1.0f;  // this produced too small steps: 0.44f;

    // Approximate yaw produced by one rotation half-step. Used to convert
    // rotate(degrees) into a half-step count; tune to your geometry.
    float degreesPerHalfRotation = 15.0f;

    // Phase durations (ms).
    uint32_t legLiftMs = 200;
    uint32_t actuateMs = 350;
    uint32_t legDropMs = 400;  // 200;
    uint32_t poseMs    = 400;

    // Overlap timing within a half-step.
    // bodyLeadInMs: how long after the lift starts before body actuation
    //   begins. Small values make the gait smoother but require the legs to
    //   be off the ground quickly.
    // bodySettleMs: minimum margin between body actuation finishing and the
    //   legs touching the ground. The actual margin may be larger if the leg
    //   lift+drop time exceeds the body actuation time.
    uint32_t bodyLeadInMs = 60;
    uint32_t bodySettleMs = 50;

    // Dance: how long each leg holds at the top before dropping back down.
    uint32_t danceHoldMs = 50;  // 120;

    // Global speed modifier applied to every motion duration. 1.0 = normal,
    // 0.5 = half speed (everything takes twice as long), 2.0 = double speed.
    // Useful for slowing the whole robot down during debugging. Must be > 0.
    float speedFactor = 1.0f;

    // Scale a duration by speedFactor. Centralised so every mover and the
    // direct-keypress handler slow down uniformly when speedFactor changes.
    uint32_t scaled(uint32_t ms) const { return (uint32_t)(ms / speedFactor); }
};

// Common fraction shorthands used across movers.
constexpr float fractionBalanced = 0.0f;
constexpr float fractionMax      = 1.0f;
constexpr float fractionMin      = -1.0f;
