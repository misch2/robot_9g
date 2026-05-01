#include "robot_face.h"

#include <math.h>

namespace {
constexpr uint16_t kBgColor    = TFT_BLACK;
constexpr uint16_t kEyeColor   = TFT_CYAN;
constexpr uint16_t kPupilColor = TFT_BLACK;
constexpr uint16_t kHighlight  = TFT_WHITE;
constexpr uint16_t kMouthColor = TFT_CYAN;

// Blink: closed eyelids from t=0..kBlinkDurMs, symmetric close-then-open.
constexpr uint32_t kBlinkDurMs    = 80;  // 180;
constexpr uint32_t kBlinkMinGapMs = 2500;
constexpr uint32_t kBlinkMaxGapMs = 10000;  // 5500;

// Pupil drift: random target inside a small disc, eased toward each frame.
constexpr uint32_t kLookMinGapMs = 1500;
constexpr uint32_t kLookMaxGapMs = 4500;
constexpr float kPupilDrift      = 12.0f;  // 6.0f;   // px max offset from center
constexpr float kPupilEase       = 0.7f;   //   0.15f;  // per-frame lerp factor, higher is snappier but more jittery

// Mouth/brow stroke thickness (for arc and line drawing).
constexpr int kArcThickness = 6;
}  // namespace

RobotFace::RobotFace() = default;

void RobotFace::begin() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(kBgColor);

    leftEye.setColorDepth(16);
    rightEye.setColorDepth(16);
    if (!leftEye.createSprite(kEyeSpriteW, kEyeSpriteH) ||
        !rightEye.createSprite(kEyeSpriteW, kEyeSpriteH)) {
        Serial.println("RobotFace: eye sprite allocation failed");
    }

    randomSeed((unsigned long)micros());
    uint32_t now = millis();
    scheduleNextBlink(now);
    scheduleNextLook(now);
    // expressionDirty starts true so the first update() draws the mouth
    // and forces an initial eye render.
}

void RobotFace::setExpression(Expression e) {
    if (e == expression) return;
    expression      = e;
    expressionDirty = true;
}

void RobotFace::cycleExpression() {
    int next = (int)expression + 1;
    if (next >= (int)Expression::_Count) next = 0;
    setExpression((Expression)next);
}

const char* RobotFace::expressionName(Expression e) {
    switch (e) {
        case Expression::Happy:
            return "Happy";
        case Expression::Neutral:
            return "Neutral";
        // case Expression::Curious:
        //     return "Curious";
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

void RobotFace::update() {
    uint32_t now = millis();
    if (now - lastUpdateMs < (uint32_t)DISPLAY_REFRESH_MS) return;
    lastUpdateMs = now;

    // --- Expression change: redraw the static mouth and force eye redraw
    // (eyebrows live inside the eye sprite, so they refresh with the eyes).
    if (expressionDirty) {
        drawMouth();
        lastOpenness    = -1.0f;
        lastPupilDX     = 999;
        lastPupilDY     = 999;
        expressionDirty = false;
    }

    // --- Blink state machine: 1.0 = fully open, 0.0 = fully closed.
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

    // --- Pupil drift: pick new target on schedule, ease toward it.
    if (now >= nextLookMs) {
        float ang     = (float)random(0, 360) * 0.0174533f;
        float r       = (float)random(0, (long)(kPupilDrift * 100.0f)) * 0.01f;
        targetPupilDX = cosf(ang) * r;
        targetPupilDY = sinf(ang) * r;
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

    drawEye(leftEye, /*isLeft=*/true, openness, pdx, pdy);
    drawEye(rightEye, /*isLeft=*/false, openness, pdx, pdy);
    leftEye.pushSprite(kLeftEyeX - kEyeSpriteW / 2, kEyeCenterY - kEyeRelCenterY);
    rightEye.pushSprite(kRightEyeX - kEyeSpriteW / 2, kEyeCenterY - kEyeRelCenterY);
}

void RobotFace::drawEye(TFT_eSprite& s, bool isLeft, float openness, int pdx, int pdy) {
    s.fillSprite(kBgColor);
    int cx = kEyeSpriteW / 2;
    int cy = kEyeRelCenterY;

    // The eyelid squashes the eye vertically. Width is fixed; height is
    // 2*ry. fillSmoothRoundRect with corner radius == ry gives a circle at
    // full open and a thin horizontal capsule when closed.
    int ry = (int)lroundf((float)kEyeRadius * fmaxf(openness, 0.0f));
    if (ry < 2) ry = 2;
    s.fillSmoothRoundRect(cx - kEyeRadius, cy - ry, kEyeRadius * 2, ry * 2, ry,
                          kEyeColor, kBgColor);

    // Hide the pupil while the lid is mostly closed so it doesn't poke out.
    if (openness > 0.4f) {
        s.fillSmoothCircle(cx + pdx, cy + pdy, kPupilRadius, kPupilColor, kEyeColor);
        s.fillSmoothCircle(cx + pdx - 4, cy + pdy - 4, 3, kHighlight, kPupilColor);
    }

    drawBrow(s, isLeft);
}

void RobotFace::drawBrow(TFT_eSprite& s, bool isLeft) {
    // The eyebrow is a thick line above the eye. Two parameters control
    // its pose: innerLift (positive raises the inner end → worried/sad,
    // negative raises the outer end → relaxed/happy) and baseLift
    // (positive raises the whole brow → surprised, negative lowers it →
    // concentrating). Inner == nasal end; outer == temple end. Which is
    // which on screen flips between left and right eye.
    constexpr int kBaseY   = 20;
    constexpr int kHalfLen = 22;
    constexpr float kThick = 5.0f;

    float innerLift = 0.0f;
    float baseLift  = 0.0f;

    switch (expression) {
        case Expression::Happy:
            // Flat, lifted — relaxed/welcoming. Any inner-down tilt here
            // pairs with the smile to read "devilish", so keep it level.
            baseLift = +3.0f;
            break;
        case Expression::Neutral:
            // Slight lift so the brow isn't crowding the eye (which reads
            // as a stare).
            baseLift = +2.0f;
            break;
        // case Expression::Curious:
        //     // Strong asymmetric raise — classic single-raised-brow look.
        //     // The other brow stays slightly lifted so it doesn't sit
        //     // angrily on the eye.
        //     baseLift = isLeft ? +8.0f : +2.0f;
        //     break;
        case Expression::Concentrating:
            // Lowered AND flat. The V-shape (inner ends down) reads as
            // angry; flat lowered reads as focused/stern.
            baseLift = -2.0f;
            break;
        case Expression::Worried:
            innerLift = +5.0f;  // ^-shape (inner ends up)
            break;
        case Expression::Sad:
            innerLift = +6.0f;
            baseLift  = -1.0f;
            break;
        case Expression::Surprised:
            baseLift = +6.0f;  // raised, flat
            break;
        default:
            break;
    }

    int cx        = kEyeSpriteW / 2;
    int browY     = kBaseY - (int)lroundf(baseLift);
    int innerEndX = isLeft ? (cx + kHalfLen) : (cx - kHalfLen);
    int outerEndX = isLeft ? (cx - kHalfLen) : (cx + kHalfLen);
    int innerEndY = browY - (int)lroundf(innerLift);
    int outerEndY = browY;

    s.drawWideLine(outerEndX, outerEndY, innerEndX, innerEndY,
                   kThick, kEyeColor, kBgColor);
}

void RobotFace::drawMouth() {
    // Clear the mouth bounding box so the previous expression's pixels go.
    int boxX = kMouthCenterX - kMouthBoxW / 2;
    int boxY = kMouthCenterY - kMouthBoxH / 2;
    tft.fillRect(boxX, boxY, kMouthBoxW, kMouthBoxH, kBgColor);

    // drawSmoothArc angle convention: 0° is at 6 o'clock (south) and
    // angles increase clockwise (so 90°=west, 180°=north, 270°=east).
    // - Smile = arc near angle 0°, circle center ABOVE the mouth.
    //   That range straddles 0/360, so we draw it as two halves.
    // - Frown = arc near angle 180°, circle center BELOW the mouth.
    //   Single non-wrapping draw.

    switch (expression) {
        case Expression::Happy: {
            // Big pronounced smile.
            int r      = 45;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY - 27;  // circle center above mouth
            int spread = 55;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              360 - spread, 360, kMouthColor, kBgColor);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor);
            break;
        }
        case Expression::Neutral: {
            // Very subtle smile (large radius, small spread = gentle curve).
            int r      = 100;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY - 95;
            int spread = 14;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              360 - spread, 360, kMouthColor, kBgColor);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor);
            break;
        }
        // case Expression::Curious: {
        //     // Slight smile, shifted right for an asymmetric look.
        //     int r      = 80;
        //     int cx     = kMouthCenterX + 6;
        //     int cy     = kMouthCenterY - 67;
        //     int spread = 22;
        //     tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
        //                       360 - spread, 360, kMouthColor, kBgColor);
        //     tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
        //                       0, spread, kMouthColor, kBgColor);
        //     break;
        // }
        case Expression::Concentrating: {
            // Flat tight line.
            int w = 50, h = 6;
            tft.fillSmoothRoundRect(kMouthCenterX - w / 2,
                                    kMouthCenterY - h / 2,
                                    w, h, h / 2, kMouthColor, kBgColor);
            break;
        }
        case Expression::Worried: {
            // Subtle frown.
            int r      = 60;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY + 45;  // circle center below mouth
            int spread = 35;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              180 - spread, 180 + spread,
                              kMouthColor, kBgColor);
            break;
        }
        case Expression::Sad: {
            // Deeper, wider frown.
            int r      = 50;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY + 38;
            int spread = 55;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              180 - spread, 180 + spread,
                              kMouthColor, kBgColor);
            break;
        }
        case Expression::Surprised: {
            // Open round mouth (ring shape).
            int r = 12;
            tft.fillSmoothCircle(kMouthCenterX, kMouthCenterY, r,
                                 kMouthColor, kBgColor);
            tft.fillSmoothCircle(kMouthCenterX, kMouthCenterY, r - 5,
                                 kBgColor, kMouthColor);
            break;
        }
        default:
            break;
    }
}

void RobotFace::scheduleNextBlink(uint32_t now) {
    nextBlinkMs = now + (uint32_t)random((long)kBlinkMinGapMs, (long)kBlinkMaxGapMs);
}

void RobotFace::scheduleNextLook(uint32_t now) {
    nextLookMs = now + (uint32_t)random((long)kLookMinGapMs, (long)kLookMaxGapMs);
}
