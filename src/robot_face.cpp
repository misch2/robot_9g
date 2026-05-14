#include "robot_face.h"

namespace {
constexpr uint16_t kBgColor    = TFT_BLACK;
constexpr uint16_t kMouthColor = TFT_CYAN;
constexpr uint16_t kNoseColor  = TFT_LIGHTGREY;

// Mouth arc stroke thickness.
constexpr int kArcThickness = 8;
}  // namespace

RobotFace::RobotFace() = default;

void RobotFace::begin() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(kBgColor);
    // No face drawn here — main.cpp paints boot status during the slow
    // inits, and the first update() after boot redraws nose + mouth
    // (expressionDirty starts true).
}

void RobotFace::showBootMessage(const char* msg) {
    tft.fillScreen(kBgColor);
    tft.setTextColor(TFT_WHITE, kBgColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.drawString(msg, 120, 120);
    // The next update() must repaint the face on top of this text.
    expressionDirty = true;
}

void RobotFace::showFatalError(const char* msg) {
    tft.fillScreen(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextFont(4);
    tft.drawString("ERROR", 120, 90);

    // Wrap long messages: the 240px round panel realistically fits ~18 chars
    // of font-4 per line. Split on spaces, draw up to three lines centered.
    constexpr int kMaxCharsPerLine = 18;
    constexpr int kLineHeight      = 28;
    const int baseY                = 130;
    int lineY                      = baseY;
    int written                    = 0;
    const char* p                  = msg ? msg : "";
    char buf[kMaxCharsPerLine + 1];
    while (*p && written < 3) {
        // Skip leading spaces.
        while (*p == ' ') p++;
        if (!*p) break;
        // Find the longest prefix that fits, preferring to break at a space.
        int len = 0;
        int lastSpace = -1;
        while (p[len] && len < kMaxCharsPerLine) {
            if (p[len] == ' ') lastSpace = len;
            len++;
        }
        int take = len;
        if (p[len] && lastSpace > 0) take = lastSpace;
        memcpy(buf, p, take);
        buf[take] = '\0';
        tft.drawString(buf, 120, lineY);
        lineY += kLineHeight;
        written++;
        p += take;
    }

    // Caller should stop calling update() — but force a repaint just in case.
    expressionDirty = true;
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

void RobotFace::update() {
    if (!expressionDirty) return;
    // Full clear + redraw — wipes any boot status text and lets drawMouth()
    // skip its own bounding-box clear concern.
    tft.fillScreen(kBgColor);
    drawNose();
    drawMouth();
    expressionDirty = false;
}

void RobotFace::drawNose() {
    // Inverted-triangle cat/dog nose. Built from two filled primitives:
    // a sharp-tipped triangle for the body, and a thin horizontal capsule
    // layered on its top edge whose rounded ends become the soft shoulders.
    const int cx    = kNoseCenterX;
    const int halfW = kNoseW / 2;
    const int topY  = kNoseCenterY - kNoseH / 2;
    const int tipY  = kNoseCenterY + kNoseH / 2;

    tft.fillTriangle(cx - halfW, topY,
                     cx + halfW, topY,
                     cx, tipY,
                     kNoseColor);

    constexpr int kShoulderCapH = 14;
    tft.fillSmoothRoundRect(cx - halfW, topY - kShoulderCapH / 2,
                            kNoseW, kShoulderCapH, kShoulderCapH / 2,
                            kNoseColor, kBgColor);
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
            int r      = 60;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY - 36;  // circle center above mouth
            int spread = 55;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              360 - spread, 360, kMouthColor, kBgColor, true);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor, true);
            break;
        }
        case Expression::Neutral: {
            // Very subtle smile (large radius, small spread = gentle curve).
            int r      = 130;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY - 123;
            int spread = 18;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              360 - spread, 360, kMouthColor, kBgColor);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor);
            break;
        }
        case Expression::Curious: {
            // Slight smile, shifted right for an asymmetric look.
            int r      = 105;
            int cx     = kMouthCenterX + 8;
            int cy     = kMouthCenterY - 88;
            int spread = 28;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              360 - spread, 360, kMouthColor, kBgColor);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor);
            break;
        }
        case Expression::Concentrating: {
            // Flat tight line.
            int w = 68, h = 8;
            tft.fillSmoothRoundRect(kMouthCenterX - w / 2,
                                    kMouthCenterY - h / 2,
                                    w, h, h / 2, kMouthColor, kBgColor);
            break;
        }
        case Expression::Worried: {
            // Subtle frown.
            int r      = 78;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY + 59;  // circle center below mouth
            int spread = 35;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              180 - spread, 180 + spread,
                              kMouthColor, kBgColor);
            break;
        }
        case Expression::Sad: {
            // Deeper, wider frown.
            int r      = 65;
            int cx     = kMouthCenterX;
            int cy     = kMouthCenterY + 50;
            int spread = 55;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              180 - spread, 180 + spread,
                              kMouthColor, kBgColor);
            break;
        }
        case Expression::Surprised: {
            // Open round mouth (ring shape).
            int r = 16;
            tft.fillSmoothCircle(kMouthCenterX, kMouthCenterY, r,
                                 kMouthColor, kBgColor);
            tft.fillSmoothCircle(kMouthCenterX, kMouthCenterY, r - 7,
                                 kBgColor, kMouthColor);
            break;
        }
        default:
            break;
    }
}
