#include "robot_face.h"

namespace {
constexpr uint16_t kBgColor    = TFT_BLACK;
constexpr uint16_t kMouthColor = TFT_CYAN;
constexpr uint16_t kNoseColor  = TFT_PINK;

// Mouth arc stroke thickness.
constexpr int kArcThickness = 8;
}  // namespace

RobotFace::RobotFace() = default;

void RobotFace::begin() {
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(kBgColor);
    // Nose never changes with expression, so draw once. drawMouth() only
    // clears the mouth bounding box, which sits below the nose region.
    drawNose();
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

void RobotFace::drawNose() {
    // Animal-style nose: rounded rectangle "shoulders" on top, tapering
    // into a triangular apex below. A thin philtrum line drops from the
    // apex toward the mouth.
    constexpr int kCornerR = 12;

    const int rectH   = (kNoseH * 6) / 10;
    const int rectTop = kNoseCenterY - kNoseH / 2;
    tft.fillSmoothRoundRect(kNoseCenterX - kNoseW / 2, rectTop,
                            kNoseW, rectH, kCornerR, kNoseColor, kBgColor);

    // Overlap the rect's bottom by kCornerR/2 so its rounded bottom
    // corners are covered by the triangle.
    const int triTop = rectTop + rectH - kCornerR / 2;
    tft.fillTriangle(kNoseCenterX - kNoseW / 2, triTop,
                     kNoseCenterX + kNoseW / 2, triTop,
                     kNoseCenterX, kNoseCenterY + kNoseH / 2,
                     kNoseColor);

    // Philtrum — thin vertical line from the nose apex toward (but not
    // into) the mouth bounding box, which gets cleared on every redraw.
    tft.drawWideLine(kNoseCenterX, kNoseCenterY + kNoseH / 2,
                     kNoseCenterX, kMouthCenterY - kMouthBoxH / 2 - 2,
                     3, kNoseColor, kBgColor);
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
