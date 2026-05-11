#include "robot_face.h"

namespace {
constexpr uint16_t kBgColor    = TFT_BLACK;
constexpr uint16_t kMouthColor = TFT_CYAN;

// Mouth/brow stroke thickness (for arc drawing).
constexpr int kArcThickness = 6;
}  // namespace

RobotFace::RobotFace() = default;

void RobotFace::begin() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(kBgColor);
    // expressionDirty starts true so the first update() draws the mouth.
}

void RobotFace::setExpression(Expression e) {
    if (e == expression) return;
    expression      = e;
    expressionDirty = true;
}

void RobotFace::cycleExpression() {
    int next = (int)expression + 1;
    if (next >= (int)Expression::_Count) next = 0;

    // FIXME tmp skip Neutral and Curious until I add some more expressions in that vein; they look weird with the current mouth set.
    if (next == (int)Expression::Neutral) next = (int)Expression::Concentrating;
    if (next == (int)Expression::Curious) next = (int)Expression::Concentrating;

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
    drawMouth();
    expressionDirty = false;
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
                              360 - spread, 360, kMouthColor, kBgColor, true);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor, true);
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
        case Expression::Curious: {
            // Slight smile, shifted right for an asymmetric look.
            int r      = 80;
            int cx     = kMouthCenterX + 6;
            int cy     = kMouthCenterY - 67;
            int spread = 22;
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              360 - spread, 360, kMouthColor, kBgColor);
            tft.drawSmoothArc(cx, cy, r, r - kArcThickness,
                              0, spread, kMouthColor, kBgColor);
            break;
        }
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
