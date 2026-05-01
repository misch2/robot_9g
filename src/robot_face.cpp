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
constexpr float kPupilEase       = 1.0;    //   0.15f;  // per-frame lerp factor
}  // namespace

RobotFace::RobotFace() = default;

void RobotFace::begin() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(kBgColor);

    leftEye.setColorDepth(16);
    rightEye.setColorDepth(16);
    if (!leftEye.createSprite(kEyeSpriteSize, kEyeSpriteSize) ||
        !rightEye.createSprite(kEyeSpriteSize, kEyeSpriteSize)) {
        Serial.println("RobotFace: eye sprite allocation failed");
    }

    // Mouth is static; draw once directly on the TFT. The eye sprites
    // never overlap this region so they won't disturb it.
    int mouthX = (TFT_WIDTH - kMouthW) / 2;
    tft.fillSmoothRoundRect(mouthX, kMouthY, kMouthW, kMouthH, kMouthH / 2, kMouthColor, kBgColor);

    randomSeed((unsigned long)micros());
    uint32_t now = millis();
    scheduleNextBlink(now);
    scheduleNextLook(now);
}

void RobotFace::update() {
    uint32_t now = millis();
    if (now - lastUpdateMs < (uint32_t)DISPLAY_REFRESH_MS) return;
    lastUpdateMs = now;

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

    drawEye(leftEye, openness, pdx, pdy);
    drawEye(rightEye, openness, pdx, pdy);
    leftEye.pushSprite(kLeftEyeX - kEyeSpriteSize / 2, kEyeCenterY - kEyeSpriteSize / 2);
    rightEye.pushSprite(kRightEyeX - kEyeSpriteSize / 2, kEyeCenterY - kEyeSpriteSize / 2);
}

void RobotFace::drawEye(TFT_eSprite& s, float openness, int pdx, int pdy) {
    s.fillSprite(kBgColor);
    int cx = kEyeSpriteSize / 2;
    int cy = kEyeSpriteSize / 2;

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
}

void RobotFace::scheduleNextBlink(uint32_t now) {
    nextBlinkMs = now + (uint32_t)random((long)kBlinkMinGapMs, (long)kBlinkMaxGapMs);
}

void RobotFace::scheduleNextLook(uint32_t now) {
    nextLookMs = now + (uint32_t)random((long)kLookMinGapMs, (long)kLookMaxGapMs);
}
