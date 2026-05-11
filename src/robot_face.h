#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Drives the GC9A01 round display, drawing the mouth (and eventually a
// nose) of the robot face. Eyes live on a separate pair of GC9D01 panels
// driven by RobotEyes — see robot_eyes.h. The Expression enum is the
// single source of truth for what facial state to render; main.cpp keeps
// both classes in lockstep by forwarding setExpression() / cycleExpression()
// to each. The class owns its TFT_eSPI instance — there is only one
// GC9A01 panel on this bus (configured via -DUSE_HSPI_PORT=1 so it doesn't
// collide with the eyes' SPI bus).
//
// Call begin() once from setup() and update() every loop iteration.
// update() only redraws on expression change, so it's almost free when
// nothing has changed.
class RobotFace {
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

    RobotFace();

    void begin();
    void update();

    void setExpression(Expression e);
    Expression getExpression() const { return expression; }
    void cycleExpression();
    static const char* expressionName(Expression e);

private:
    // Geometry. The TFT is 240x240; nose sits in the upper-middle area
    // and the mouth in the lower area, both centered horizontally.
    static constexpr int kNoseCenterX = 120;
    static constexpr int kNoseCenterY = 95;
    static constexpr int kNoseW       = 64;
    static constexpr int kNoseH       = 48;

    static constexpr int kMouthCenterX = 120;
    static constexpr int kMouthCenterY = 185;
    static constexpr int kMouthBoxW    = 180;
    static constexpr int kMouthBoxH    = 80;

    TFT_eSPI tft;

    Expression expression = Expression::Happy;
    bool expressionDirty  = true;  // forces full redraw on first frame

    void drawNose();
    void drawMouth();
};
