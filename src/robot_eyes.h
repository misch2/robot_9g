#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Drives two GC9D01 160x160 round displays — one per eye — as an
// alternative to RobotFace's single GC9A01. There's no mouth: expression
// is conveyed through eye shape (squint, widen, droop) plus eyebrows.
//
// Hardware: shared SPI bus (DC, MOSI, SCLK), per-display CS and RST.
// TFT_eSPI is built with TFT_CS=-1 / TFT_RST=-1 so the lib never touches
// those lines; this class drives them as plain GPIO and toggles the
// active CS around each pushSprite. Both displays receive the same init
// sequence (one at a time, with the inactive one CS-high).
//
// Public API mirrors RobotFace 1:1 (same Expression enum, same method
// signatures) so main.cpp can pick between them via `using RobotFace =
// RobotEyes;` under -DROBOT_FACE_VARIANT_EYES.
class RobotEyes {
public:
    enum class Expression : uint8_t {
        Happy,
        Neutral,
        Curious,
        Concentrating,
        Worried,
        Sad,
        Surprised,
        _Count
    };

    RobotEyes();

    void begin();
    void update();

    void setExpression(Expression e);
    Expression getExpression() const { return expression; }
    void cycleExpression();
    static const char* expressionName(Expression e);

private:
    // Each display is 160x160; the eye is centered with headroom above
    // for an eyebrow.
    static constexpr int kDisplayW    = 160;
    static constexpr int kDisplayH    = 160;
    static constexpr int kEyeCenterX  = 80;
    static constexpr int kEyeCenterY  = 92;
    static constexpr int kEyeRadius   = 50;
    static constexpr int kPupilRadius = 18;

    TFT_eSPI tft;
    // One sprite reused for both eyes — drawn, pushed to display 0,
    // redrawn for the right eye, pushed to display 1.
    TFT_eSprite eyeSprite{&tft};

    // Per-display CS/RST pins. Index 0 == left eye, 1 == right eye.
    int csPin[2]  = {PIN_TFT_CS1, PIN_TFT_CS2};
    int rstPin[2] = {PIN_TFT_RST1, PIN_TFT_RST2};

    Expression expression = Expression::Happy;
    bool expressionDirty  = true;

    uint32_t lastUpdateMs = 0;

    // Blink
    uint32_t blinkStartMs = 0;
    uint32_t nextBlinkMs  = 0;
    bool blinking         = false;

    // Pupil drift (eased toward target)
    float pupilDX       = 0.0f;
    float pupilDY       = 0.0f;
    float targetPupilDX = 0.0f;
    float targetPupilDY = 0.0f;
    uint32_t nextLookMs = 0;

    // Last-frame state for change detection (sentinel forces first draw).
    float lastOpenness = -1.0f;
    int lastPupilDX    = 999;
    int lastPupilDY    = 999;

    // Per-expression eye-shape modifiers, computed in setExpression.
    // Eye is drawn as a vertical capsule of width 2*kEyeRadius and height
    // 2*ry; eyeRadiusScale scales ry. eyelidArc swaps the upper half for a
    // downward arc → ^_^ happy-squint look. lidDroop closes the lid from
    // the top by a fraction of the eye height.
    float eyeRadiusScale = 1.0f;
    float lidDroop       = 0.0f;
    bool eyelidArc       = false;
    float pupilDYBias    = 0.0f;
    float pupilScale     = 1.0f;

    void selectDisplay(int idx);
    void deselectAll();
    void initOneDisplay(int idx);

    void drawEye(bool isLeft, float openness, int pdx, int pdy);
    void drawBrow(bool isLeft);
    void applyExpressionModifiers();
    void scheduleNextBlink(uint32_t now);
    void scheduleNextLook(uint32_t now);
};
