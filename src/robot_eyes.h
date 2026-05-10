#pragma once

#include <Arduino.h>
#include <GC9D01_LTSM.hpp>
#include <TFT_eSPI.h>

// Drives two GC9D01 160x160 round displays — one per eye — as an
// alternative to RobotFace's single GC9A01. There's no mouth: expression
// is conveyed through eye shape (squint, widen, droop) plus eyebrows.
//
// Render pipeline: TFT_eSPI's TFT_eSprite owns an offscreen RGB565
// buffer where the eye + brow are drawn with the lib's antialiased
// primitives (fillSmoothRoundRect / fillSmoothCircle / drawWideLine).
// At 16bpp, TFT_eSprite pre-byte-swaps every write before storing, so
// the buffer is already MSB-first in memory — exactly what the panel
// expects on the wire. Each frame, after rendering one eye, the buffer
// is handed verbatim to GC9D01_LTSM::drawBitmap16Data, which streams it
// over SPI with that panel's CS asserted. Then the sprite is re-rendered
// for the second eye and pushed to the second panel.
//
// Hardware: shared SPI bus (DC, MOSI, SCLK), per-panel CS and RST. Each
// GC9D01_LTSM instance owns its own CS/RST/DC pins and asserts CS only
// during its own SPI transactions; the DC line is driven by whichever
// instance is currently transmitting. We pre-call SPI.begin() with our
// custom MOSI/SCLK pins before constructing the panels so the lib's
// internal SPI.begin() (no-arg, called inside TFTGC9D01Initialize) is a
// no-op due to arduino-esp32's _spi guard.
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

    // Set the MADCTL rotation applied to BOTH panels. Same rotation on
    // both = same tint (cheap GC9D01 modules show a subtle per-MADCTL
    // tint shift driven by MX/MY/ML bits asymmetrically biasing the
    // panel's gamma response per scan direction). To compensate for
    // the panels being mounted 180° opposed, panel 1's sprite buffer
    // is always 180°-flipped in software before push — this works
    // regardless of the chosen rotation. Only Degrees_90 yields
    // naturally upright eyes; other values produce matching but
    // rotated orientations. Treat this as an aesthetic tint knob.
    void setRotation(GC9D01_LTSM::display_rotate_e r);
    GC9D01_LTSM::display_rotate_e getRotation() const { return rotation; }

    // Debug helper: blit assets/eye1_data.h onto both panels, with the
    // right eye horizontally flipped. Freezes the expression renderer
    // until setExpression() / cycleExpression() is called again.
    void showTestImage();

private:
    // Each display is 160x160; the eye is centered with headroom above
    // for an eyebrow.
    static constexpr int kDisplayW    = 160;
    static constexpr int kDisplayH    = 160;
    static constexpr int kEyeCenterX  = 80;
    static constexpr int kEyeCenterY  = 92;
    static constexpr int kEyeRadius   = 50;
    static constexpr int kPupilRadius = 18;

    // tft owns no panel — only exists so eyeSprite has a parent for
    // color helpers and font tables. We never call tft.init() or
    // pushSprite; SPI is owned entirely by the GC9D01_LTSM panels.
    TFT_eSPI tft;
    // One sprite reused for both eyes — drawn, pushed to panel 0,
    // redrawn for the right eye, pushed to panel 1.
    TFT_eSprite eyeSprite{&tft};

    // Index 0 == left eye, 1 == right eye. Each panel owns its own
    // CS/RST/DC pins (DC is shared across both physically — both
    // instances drive the same GPIO, harmless since only one CS is
    // asserted at a time).
    GC9D01_LTSM panel[2];

    // Shared MADCTL rotation for both panels. Default Degrees_0 gives
    // the purest colors (no MX/MY/ML bits set) — see the setRotation
    // comment above for the tint story. The 180° SW flip on panel 1
    // keeps both eyes' orientations matching at any rotation.
    GC9D01_LTSM::display_rotate_e rotation = GC9D01_LTSM::Degrees_0;

    Expression expression = Expression::Happy;
    bool expressionDirty  = true;

    // When true, update() suspends animation so the test image stays put.
    bool showingTestImage = false;

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

    void initOneDisplay(int idx);
    void pushSprite(int idx, bool alreadyRotated = false);

    void drawEye(bool isLeft, float openness, int pdx, int pdy);
    void drawBrow(bool isLeft);
    void applyExpressionModifiers();
    void scheduleNextBlink(uint32_t now);
    void scheduleNextLook(uint32_t now);
};
