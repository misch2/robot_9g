#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Drives the GC9A01 round display, drawing an animated robot face: two
// cyan eyes that blink on a randomized interval and pupils that drift to
// "look around". An Expression enum drives a static eyebrow + mouth pose
// on top of that — set/cycle it to make the face look happy, worried,
// curious, etc. The class owns the TFT_eSPI instance — there is only one
// display and the rest of the codebase follows the same own-your-
// resources pattern (see ServoManager).
//
// Call begin() once from setup() and update() every loop iteration.
// update() is internally throttled to DISPLAY_REFRESH_MS and only pushes
// pixels when the visual state actually changed, so it doesn't starve
// servo updates running in the same loop.
class RobotFace {
public:
    enum class Expression : uint8_t {
        Happy,
        Neutral,
        // Curious,
        Concentrating,
        Worried,
        Sad,
        Surprised,
        _Count
    };

    RobotFace();

    void begin();
    void update();

    void setExpression(Expression e);
    Expression getExpression() const { return expression; }
    void cycleExpression();
    static const char* expressionName(Expression e);

private:
    // Geometry. The TFT is 240x240; everything is laid out around the
    // optical center at (120, 120).
    static constexpr int kEyeRadius   = 30;
    static constexpr int kPupilRadius = 12;
    static constexpr int kEyeCenterY  = 105;
    static constexpr int kLeftEyeX    = 80;
    static constexpr int kRightEyeX   = 160;
    // Eye sprite carries the eye + eyebrow above it. Width matches the
    // eye spacing so the two sprites abut without overlap; height adds
    // headroom for the eyebrow above the eye outline.
    static constexpr int kEyeSpriteW    = 80;
    static constexpr int kEyeSpriteH    = 100;
    static constexpr int kEyeRelCenterY = 60;

    // Mouth lives below the eye sprites and is drawn directly on the TFT
    // (not double-buffered). It only changes on expression changes, so a
    // brief clear-and-redraw is acceptable.
    static constexpr int kMouthCenterX = 120;
    static constexpr int kMouthCenterY = 175;
    static constexpr int kMouthBoxW    = 140;
    static constexpr int kMouthBoxH    = 60;

    TFT_eSPI tft;
    TFT_eSprite leftEye{&tft};
    TFT_eSprite rightEye{&tft};

    Expression expression = Expression::Happy;
    bool expressionDirty  = true;  // forces full redraw on first frame

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

    void drawEye(TFT_eSprite& sprite, bool isLeft, float openness, int pdx, int pdy);
    void drawBrow(TFT_eSprite& sprite, bool isLeft);
    void drawMouth();
    void scheduleNextBlink(uint32_t now);
    void scheduleNextLook(uint32_t now);
};
