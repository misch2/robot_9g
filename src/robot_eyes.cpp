#include "robot_eyes.h"

#include <SPI.h>
#include <math.h>

namespace {
constexpr uint16_t kBgColor    = TFT_BLACK;
constexpr uint16_t kEyeColor   = TFT_CYAN;
constexpr uint16_t kPupilColor = TFT_BLACK;
constexpr uint16_t kHighlight  = TFT_WHITE;

// Blink cadence — same values as RobotFace; close-then-open across the
// duration so peak closure is at the midpoint.
constexpr uint32_t kBlinkDurMs    = 80;
constexpr uint32_t kBlinkMinGapMs = 2500;
constexpr uint32_t kBlinkMaxGapMs = 10000;

// Pupil drift — slightly larger excursion than RobotFace because the eye
// itself is bigger here (50px radius vs 30px).
constexpr uint32_t kLookMinGapMs = 1500;
constexpr uint32_t kLookMaxGapMs = 4500;
constexpr float kPupilDrift      = 16.0f;
constexpr float kPupilEase       = 0.7f;

// Same SPI freq we used with TFT_eSPI for these panels. GC9D01 spec is
// good to ~50 MHz; 40 MHz is comfortable headroom on the ESP32-S3 SPI
// bus given the wiring (no length compensation, no MISO).
constexpr uint32_t kSpiFreqHz = 40000000;

// Per-eye CS/RST. PIN_TFT_DC is shared (single GPIO line into both
// panels). Index 0 == left eye, 1 == right eye.
constexpr int kCsPin[2]  = {PIN_TFT_CS1, PIN_TFT_CS2};
constexpr int kRstPin[2] = {PIN_TFT_RST1, PIN_TFT_RST2};

// Which panel needs its sprite 180°-flipped in software. Panels are
// physically mounted 180° opposed, so with both at the same MADCTL
// rotation one of them comes out 180° rotated visually. Flipping that
// one in SW lines them back up. With default rotation Degrees_90 it's
// panel 1 that needs the flip; if you swap which physical eye is wired
// to CS1 vs CS2, change this index.
constexpr int kFlipPanel = 1;

// In-place 180° rotation of a square (or any) RGB565 buffer. Equivalent
// to reversing the pixel array — for a square sprite, pixel (x, y)
// maps to (W-1-x, H-1-y), which in a row-major array of N²=W*H pixels
// is index (N-1-i). 12,800 swap iterations on the 160² buffer is
// well under a millisecond on the ESP32-S3.
inline void rotate180InPlace(uint16_t* buf, size_t pixels) {
    for (size_t i = 0, j = pixels - 1; i < j; ++i, --j) {
        uint16_t t = buf[i];
        buf[i]     = buf[j];
        buf[j]     = t;
    }
}
}  // namespace

RobotEyes::RobotEyes() = default;

void RobotEyes::begin() {
    // Pin the SPI bus to our wiring before any panel touches it; the
    // arg-less SPI.begin() inside GC9D01_LTSM::TFTHWSPIInitialize will
    // then short-circuit on the _spi-already-initialised guard and
    // leave our pin choices intact.
    SPI.begin(TFT_SCLK, /*miso=*/-1, TFT_MOSI, /*ss=*/-1);

    initOneDisplay(0);
    initOneDisplay(1);

    eyeSprite.setColorDepth(16);
    if (!eyeSprite.createSprite(kDisplayW, kDisplayH)) {
        Serial.println("RobotEyes: eye sprite allocation failed");
    }

    randomSeed((unsigned long)micros());
    uint32_t now = millis();
    scheduleNextBlink(now);
    scheduleNextLook(now);

    applyExpressionModifiers();
}

void RobotEyes::initOneDisplay(int idx) {
    panel[idx].TFTsetupGPIO_SPI(kSpiFreqHz, kRstPin[idx], TFT_DC, kCsPin[idx]);
    panel[idx].TFTInitScreenSize(kDisplayW, kDisplayH,
                                 GC9D01_LTSM::Resolution_e::RGB160x160_DualGate,
                                 GC9D01_LTSM::PixelFixMode_e::Both);
    panel[idx].TFTGC9D01Initialize();
    panel[idx].TFTsetRotation(rotation);
    panel[idx].fillScreen(kBgColor);
}

void RobotEyes::setRotation(GC9D01_LTSM::display_rotate_e r) {
    if (rotation == r) return;
    rotation = r;
    panel[0].TFTsetRotation(r);
    panel[1].TFTsetRotation(r);
    // Force a redraw next update — the new MADCTL also clears any
    // address-window state, and the prior frame might be stale.
    expressionDirty = true;
    lastOpenness    = -1.0f;
    lastPupilDX     = 999;
    lastPupilDY     = 999;
}

void RobotEyes::pushSprite(int idx) {
    // TFT_eSprite at 16bpp pre-byte-swaps every write (drawPixel /
    // fillRect / fillSmoothCircle ... all do `color = (color >> 8) |
    // (color << 8)` before storing — see Extensions/Sprite.cpp:1642).
    // So the sprite buffer is already laid out MSB-first in memory,
    // which is exactly the byte order GC9D01_LTSM streams to the
    // panel — hand it over verbatim, no swap.
    uint16_t* buf = static_cast<uint16_t*>(eyeSprite.getPointer());
    if (!buf) return;
    if (idx == kFlipPanel) {
        rotate180InPlace(buf, (size_t)kDisplayW * kDisplayH);
    }
    panel[idx].drawBitmap16Data(0, 0, reinterpret_cast<const uint8_t*>(buf),
                                kDisplayW, kDisplayH);
}

void RobotEyes::setExpression(Expression e) {
    if (e == expression) return;
    expression      = e;
    expressionDirty = true;
    applyExpressionModifiers();
}

void RobotEyes::cycleExpression() {
    int next = (int)expression + 1;
    if (next >= (int)Expression::_Count) next = 0;
    setExpression((Expression)next);
}

const char* RobotEyes::expressionName(Expression e) {
    switch (e) {
        case Expression::Happy:
            return "Happy";
        case Expression::Neutral:
            return "Neutral";
        case Expression::Curious:
            return "Curious";
        case Expression::Concentrating:
            return "Concentrating";
        case Expression::Worried:
            return "Worried";
        case Expression::Sad:
            return "Sad";
        case Expression::Surprised:
            return "Surprised";
        default:
            return "?";
    }
}

void RobotEyes::applyExpressionModifiers() {
    // Defaults — neutral round eye.
    eyeRadiusScale = 1.0f;
    lidDroop       = 0.0f;
    eyelidArc      = false;
    pupilDYBias    = 0.0f;
    pupilScale     = 1.0f;

    switch (expression) {
        case Expression::Happy:
            // Carve the upper half of the eye into a downward arc — the
            // ^_^ smile-eye look. Brow is suppressed for this one.
            eyelidArc = true;
            break;
        case Expression::Neutral:
        case Expression::Curious:
            // Eyes round; expression carried by the brow.
            break;
        case Expression::Concentrating:
            // Vertically squashed → focused squint.
            eyeRadiusScale = 0.6f;
            break;
        case Expression::Worried:
            // Round eyes, ^-shape brow does the work.
            break;
        case Expression::Sad:
            // Upper lid covers ~40% from the top, pupils slide down.
            lidDroop    = 0.4f;
            pupilDYBias = 6.0f;
            break;
        case Expression::Surprised:
            // Slightly larger eye, smaller pupil — the classic look.
            eyeRadiusScale = 1.15f;
            pupilScale     = 0.7f;
            break;
        default:
            break;
    }
}

void RobotEyes::update() {
    uint32_t now = millis();
    if (now - lastUpdateMs < (uint32_t)DISPLAY_REFRESH_MS) return;
    lastUpdateMs = now;

    // Expression change forces a redraw on the next pass.
    if (expressionDirty) {
        lastOpenness    = -1.0f;
        lastPupilDX     = 999;
        lastPupilDY     = 999;
        expressionDirty = false;
    }

    // Blink state machine: 1.0 = fully open, 0.0 = fully closed.
    if (!blinking && now >= nextBlinkMs) {
        blinking     = true;
        blinkStartMs = now;
    }
    float openness = 1.0f;
    if (blinking) {
        uint32_t t = now - blinkStartMs;
        if (t >= kBlinkDurMs) {
            blinking = false;
            scheduleNextBlink(now);
        } else {
            float p  = (float)t / (float)kBlinkDurMs;
            openness = (p < 0.5f) ? (1.0f - 2.0f * p) : (2.0f * p - 1.0f);
        }
    }

    // Pupil drift target (random within disc), eased toward each frame.
    if (now >= nextLookMs) {
        float ang     = (float)random(0, 360) * 0.0174533f;
        float r       = (float)random(0, (long)(kPupilDrift * 100.0f)) * 0.01f;
        targetPupilDX = cosf(ang) * r;
        targetPupilDY = sinf(ang) * r + pupilDYBias;
        scheduleNextLook(now);
    }
    pupilDX += (targetPupilDX - pupilDX) * kPupilEase;
    pupilDY += (targetPupilDY - pupilDY) * kPupilEase;

    int pdx = (int)lroundf(pupilDX);
    int pdy = (int)lroundf(pupilDY);
    if (openness == lastOpenness && pdx == lastPupilDX && pdy == lastPupilDY) return;
    lastOpenness = openness;
    lastPupilDX  = pdx;
    lastPupilDY  = pdy;

    // Reuse the single sprite: draw left, push to panel 0, redraw for
    // the right eye, push to panel 1. Saves a 51 kB second framebuffer.
    drawEye(/*isLeft=*/true, openness, pdx, pdy);
    pushSprite(0);

    drawEye(/*isLeft=*/false, openness, pdx, pdy);
    pushSprite(1);
}

void RobotEyes::drawEye(bool isLeft, float openness, int pdx, int pdy) {
    eyeSprite.fillSprite(kBgColor);
    int cx = kEyeCenterX;
    int cy = kEyeCenterY;

    // ry collapses with blink; eyeRadiusScale lets expressions widen or
    // squash the eye independently of the blink animation.
    int ry = (int)lroundf((float)kEyeRadius * fmaxf(openness, 0.0f) * eyeRadiusScale);
    if (ry < 2) ry = 2;
    int rx = kEyeRadius;

    // Capsule (round-rect with corner radius == ry). At full open this is
    // a circle; at blink-closed it's a thin horizontal slit.
    eyeSprite.fillSmoothRoundRect(cx - rx, cy - ry, rx * 2, ry * 2, ry,
                                  kEyeColor, kBgColor);

    // Happy: carve the top with a BG-coloured circle offset upward,
    // leaving a downward crescent → smiling eye.
    if (eyelidArc) {
        int carveR = (int)(rx * 1.05f);
        int carveY = cy - (int)(ry * 0.4f);
        eyeSprite.fillSmoothCircle(cx, carveY, carveR, kBgColor, kBgColor);
    }

    // Sad / sleepy: upper lid drops by a fraction of the eye height.
    if (lidDroop > 0.0f) {
        int droopH = (int)lroundf(ry * 2.0f * lidDroop);
        eyeSprite.fillRect(cx - rx, cy - ry, rx * 2, droopH, kBgColor);
    }

    // Pupil — hide while the lid is mostly closed or while the happy
    // crescent has eaten the upper half (no good pupil position there).
    if (openness > 0.4f && !eyelidArc) {
        int pr = (int)lroundf(kPupilRadius * pupilScale);
        if (pr < 3) pr = 3;
        eyeSprite.fillSmoothCircle(cx + pdx, cy + pdy, pr, kPupilColor, kEyeColor);
        eyeSprite.fillSmoothCircle(cx + pdx - 5, cy + pdy - 5, 4, kHighlight, kPupilColor);
    }

    drawBrow(isLeft);
}

void RobotEyes::drawBrow(bool isLeft) {
    // Brow lives near the top of the display. Two parameters: innerLift
    // (raises the inner end → worried/sad), baseLift (raises the whole
    // brow → surprised; lowers it → concentrating). Inner == nasal end;
    // which side that is on screen flips between left and right eye.
    constexpr int kBaseY   = 25;
    constexpr int kHalfLen = 38;
    constexpr float kThick = 7.0f;

    float innerLift = 0.0f;
    float baseLift  = 0.0f;

    switch (expression) {
        case Expression::Happy:
            // Brow suppressed — the carved eye crescent carries the whole expression.
            return;
        case Expression::Neutral:
            baseLift = +2.0f;
            break;
        case Expression::Curious:
            baseLift = isLeft ? +10.0f : +2.0f;
            break;
        case Expression::Concentrating:
            baseLift = -2.0f;
            break;
        case Expression::Worried:
            innerLift = +6.0f;
            break;
        case Expression::Sad:
            innerLift = +8.0f;
            baseLift  = -1.0f;
            break;
        case Expression::Surprised:
            baseLift = +8.0f;
            break;
        default:
            break;
    }

    int cx        = kEyeCenterX;
    int browY     = kBaseY - (int)lroundf(baseLift);
    int innerEndX = isLeft ? (cx + kHalfLen) : (cx - kHalfLen);
    int outerEndX = isLeft ? (cx - kHalfLen) : (cx + kHalfLen);
    int innerEndY = browY - (int)lroundf(innerLift);
    int outerEndY = browY;

    eyeSprite.drawWideLine(outerEndX, outerEndY, innerEndX, innerEndY,
                           kThick, kEyeColor, kBgColor);
}

void RobotEyes::scheduleNextBlink(uint32_t now) {
    nextBlinkMs = now + (uint32_t)random((long)kBlinkMinGapMs, (long)kBlinkMaxGapMs);
}

void RobotEyes::scheduleNextLook(uint32_t now) {
    nextLookMs = now + (uint32_t)random((long)kLookMinGapMs, (long)kLookMaxGapMs);
}
