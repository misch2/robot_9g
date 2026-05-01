#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Drives the GC9A01 round display, drawing an animated robot face: two
// cyan eyes that blink on a randomized interval, with pupils that drift
// to "look around". The class owns the TFT_eSPI instance — there is only
// one display and the rest of the codebase follows the same own-your-
// resources pattern (see ServoManager).
//
// Call begin() once from setup() and update() every loop iteration.
// update() is internally throttled to DISPLAY_REFRESH_MS and only pushes
// pixels when the visual state actually changed, so it doesn't starve
// servo updates running in the same loop.
class RobotFace {
public:
    RobotFace();

    void begin();
    void update();

private:
    // Geometry. The TFT is 240x240; everything is laid out around the
    // optical center at (120, 120).
    static constexpr int kEyeRadius     = 30;
    static constexpr int kPupilRadius   = 12;
    static constexpr int kEyeCenterY    = 105;
    static constexpr int kLeftEyeX      = 82;
    static constexpr int kRightEyeX     = 158;
    static constexpr int kEyeSpriteSize = 80;  // > 2 * (radius + pupilDrift)
    static constexpr int kMouthW        = 64;
    static constexpr int kMouthH        = 12;
    static constexpr int kMouthY        = 178;

    TFT_eSPI tft;
    TFT_eSprite leftEye{&tft};
    TFT_eSprite rightEye{&tft};

    // Pacing
    uint32_t lastUpdateMs = 0;

    // Blink state
    uint32_t blinkStartMs = 0;
    uint32_t nextBlinkMs  = 0;
    bool blinking         = false;

    // Pupil drift: float for smooth easing, int rounded for compare/draw.
    float pupilDX       = 0.0f;
    float pupilDY       = 0.0f;
    float targetPupilDX = 0.0f;
    float targetPupilDY = 0.0f;
    uint32_t nextLookMs = 0;

    // Last drawn frame state — used to skip rendering when nothing changed.
    // Sentinel values force the first frame to draw.
    float lastOpenness = -1.0f;
    int lastPupilDX    = 999;
    int lastPupilDY    = 999;

    void drawEye(TFT_eSprite& sprite, float openness, int pdx, int pdy);
    void scheduleNextBlink(uint32_t now);
    void scheduleNextLook(uint32_t now);
};
