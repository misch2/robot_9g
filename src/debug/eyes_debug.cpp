// Standalone debug sketch for the dual-GC9D01 wiring. Excluded from the
// main robot build via build_src_filter — see platformio.ini's
// [env:esp32s3_eyes_debug].
//
// Mirrors the production render path used by RobotEyes:
//   • GC9D01_LTSM owns each panel (init, address windows, pixel push).
//   • TFT_eSPI is included only for TFT_eSprite — the offscreen 160×160
//     RGB565 buffer where antialiased shapes are drawn — and is never
//     init()'d as a panel itself.
//   • Each frame: render → byte-swap to MSB-first → drawBitmap16Data.
//
// The earlier version of this sketch debugged a hand-ported GC9D01
// driver in our vendored TFT_eSPI fork; with GC9D01_LTSM doing the panel
// work properly, the alt-init / config-probe / dual-gate tests are gone.
//
//   pio run -e esp32s3_eyes_debug -t upload -t monitor
//
// Then press '?' for the menu.

#include <Arduino.h>
#include <GC9D01_LTSM.hpp>
#include <SPI.h>
#include <TFT_eSPI.h>

namespace {

constexpr int CS[2]  = {PIN_TFT_CS1, PIN_TFT_CS2};
constexpr int RST[2] = {PIN_TFT_RST1, PIN_TFT_RST2};

constexpr int W = TFT_WIDTH;
constexpr int H = TFT_HEIGHT;

constexpr uint32_t kSpiFreqHz = 40000000;

// Both panels share one MADCTL rotation so they share the same tint
// (cheap GC9D01 modules show subtle color shifts depending on MX/MY/ML
// bits — same bits both sides, same shift). To compensate for the
// panels being mounted 180° opposed, panel 1's content gets a 180° SW
// flip in test 8 (matches RobotEyes' production path). Tests 1–7 draw
// directly via panel.fillRect/fillCircle/etc, so the two physical eyes
// will show the same content rotated 180° relative to each other —
// that's expected. The 't' key cycles this rotation through all four
// values so you can pick whichever tint you like.
GC9D01_LTSM::display_rotate_e commonRotation = GC9D01_LTSM::Degrees_0;

// Which panel gets 180°-flipped in software for sprite-path tests.
// Mirrors RobotEyes' kFlipPanel.
constexpr int kFlipPanel = 1;

GC9D01_LTSM panel[2];

// TFT_eSPI exists only for TFT_eSprite — never init()'d. Sprite is
// allocated lazily so we don't pay the 51 KB unless a sprite-path test
// actually runs.
TFT_eSPI tft;
TFT_eSprite sprite{&tft};
bool spriteAllocated = false;

void initOne(int idx) {
    panel[idx].TFTsetupGPIO_SPI(kSpiFreqHz, RST[idx], TFT_DC, CS[idx]);
    panel[idx].TFTInitScreenSize(W, H,
                                 GC9D01_LTSM::Resolution_e::RGB160x160_DualGate,
                                 GC9D01_LTSM::PixelFixMode_e::Both);
    panel[idx].TFTGC9D01Initialize();
    panel[idx].TFTsetRotation(commonRotation);
    panel[idx].fillScreen(panel[idx].C_BLACK);
}

inline void rotate180InPlace(uint16_t* buf, size_t pixels) {
    for (size_t i = 0, j = pixels - 1; i < j; ++i, --j) {
        uint16_t t = buf[i];
        buf[i]     = buf[j];
        buf[j]     = t;
    }
}

void initBoth() {
    initOne(0);
    initOne(1);
}

bool ensureSprite() {
    if (spriteAllocated) return true;
    sprite.setColorDepth(16);
    if (!sprite.createSprite(W, H)) {
        Serial.println("  sprite alloc FAILED");
        return false;
    }
    spriteAllocated = true;
    return true;
}

// Push the sprite buffer to one panel — same path RobotEyes uses in
// production. TFT_eSprite at 16bpp pre-byte-swaps every write before
// storing (Sprite.cpp:1642), so the buffer is already MSB-first in
// memory and goes to drawBitmap16Data verbatim. The kFlipPanel index
// gets a 180° in-place rotation so its visual orientation matches the
// other eye despite the physical mounting being 180° opposed.
void pushSpriteTo(int idx) {
    uint16_t* buf = static_cast<uint16_t*>(sprite.getPointer());
    if (!buf) return;
    if (idx == kFlipPanel) {
        rotate180InPlace(buf, (size_t)W * H);
    }
    panel[idx].drawBitmap16Data(0, 0, reinterpret_cast<const uint8_t*>(buf), W, H);
}

// Rapid color cycle on the given panel — sanity check that pixel writes
// actually land.
void colorCycle(int idx, uint32_t totalMs) {
    static const uint16_t colors[] = {
        panel[0].C_RED, panel[0].C_GREEN, panel[0].C_BLUE,
        panel[0].C_YELLOW, panel[0].C_MAGENTA, panel[0].C_CYAN, panel[0].C_WHITE,
    };
    constexpr int N = sizeof(colors) / sizeof(colors[0]);
    uint32_t per   = 500;
    uint32_t start = millis();
    int i          = 0;
    while (millis() - start < totalMs) {
        panel[idx].fillScreen(colors[i % N]);
        Serial.printf("  color %d\n", i % N);
        delay(per);
        i++;
    }
    panel[idx].fillScreen(panel[idx].C_BLACK);
}

// Geometric per-panel ID — no text fonts needed. Panel 0 → red bg, big
// concentric squares. Panel 1 → blue bg, big concentric circles.
void drawIdPattern(int idx) {
    GC9D01_LTSM& p = panel[idx];
    uint16_t bg    = (idx == 0) ? p.C_RED : p.C_BLUE;
    p.fillScreen(bg);
    p.drawRectWH(0, 0, W, H, p.C_WHITE);
    p.drawRectWH(4, 4, W - 8, H - 8, p.C_WHITE);
    int cx = W / 2, cy = H / 2;
    if (idx == 0) {
        // Stack of concentric squares.
        for (int s = 60; s > 8; s -= 12)
            p.drawRectWH(cx - s / 2, cy - s / 2, s, s, p.C_WHITE);
    } else {
        // Stack of concentric circles.
        for (int r = 30; r > 4; r -= 6) p.drawCircle(cx, cy, r, p.C_WHITE);
    }
}

// --- Tests ------------------------------------------------------------------

void test1_displayAlone(int idx) {
    Serial.printf("\n[Test] Display %d alone — color cycle then ID\n", idx);
    initBoth();
    Serial.println("  cycling colors for 5s");
    colorCycle(idx, 5000);
    drawIdPattern(idx);
    Serial.println("  done — id pattern left on screen");
}

void test2_csIsolation() {
    Serial.println("\n[Test] CS isolation — init both, distinct id patterns");
    initBoth();
    drawIdPattern(0);
    drawIdPattern(1);
    Serial.println("  done — D0: RED + concentric squares, D1: BLUE + circles");
    Serial.println("  if both look the same → CS isolation is failing");
    Serial.println("  if one is blank      → that panel didn't init");
}

void test3_alternating() {
    Serial.println("\n[Test] Alternating fillScreen for 5s — CS toggle stress");
    initBoth();
    uint32_t start = millis();
    int frame      = 0;
    while (millis() - start < 5000) {
        bool a = frame & 1;
        panel[0].fillScreen(a ? panel[0].C_RED : panel[0].C_GREEN);
        panel[1].fillScreen(a ? panel[1].C_BLUE : panel[1].C_YELLOW);
        delay(200);
        frame++;
    }
    Serial.println("  done");
}

void test4_addressWindow() {
    Serial.println("\n[Test] Address-window walk — colored quadrants");
    initBoth();
    for (int idx = 0; idx < 2; ++idx) {
        GC9D01_LTSM& p = panel[idx];
        p.fillScreen(p.C_BLACK);
        p.fillRect(0, 0, W / 2, H / 2, p.C_RED);
        p.fillRect(W / 2, 0, W / 2, H / 2, p.C_GREEN);
        p.fillRect(0, H / 2, W / 2, H / 2, p.C_BLUE);
        p.fillRect(W / 2, H / 2, W / 2, H / 2, p.C_WHITE);
        p.drawLine(0, 0, W - 1, H - 1, p.C_YELLOW);
        p.drawLine(0, H - 1, W - 1, 0, p.C_YELLOW);
    }
    Serial.println("  done — each panel: 4 colored quadrants + diagonals");
    Serial.println("  if rects look shifted/wrapped → address-window math is wrong");
}

void test5_invertOn() {
    Serial.println("\n[Test] Invert ON — sanity check on panel polarity");
    initBoth();
    for (int idx = 0; idx < 2; ++idx) {
        panel[idx].TFTchangeInvertMode(true);
        drawIdPattern(idx);
    }
    Serial.println("  done — press 'i' to toggle back if colors look wrong");
}

// --- Pattern gallery -------------------------------------------------------
// Per-primitive isolation. If something draws wrong, the per-pattern label
// narrows down which code path is broken.

void pat_hairlinesV(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int x = 0; x < W; x += step) p.drawFastVLine(x, 0, H, fg);
    for (int x = 0; x < W; x += 10) {
        p.drawFastHLine(x, 0, 4, p.C_YELLOW);
        p.drawFastHLine(x, H - 4, 4, p.C_YELLOW);
    }
}

void pat_hairlinesH(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int y = 0; y < H; y += step) p.drawFastHLine(0, y, W, fg);
    for (int y = 0; y < H; y += 10) {
        p.drawFastVLine(0, y, 4, p.C_YELLOW);
        p.drawFastVLine(W - 4, y, 4, p.C_YELLOW);
    }
}

void pat_pixelGrid(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int y = 0; y < H; y += step)
        for (int x = 0; x < W; x += step) p.drawPixel(x, y, fg);
}

void pat_concentricRects(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    int cx = W / 2, cy = H / 2;
    for (int d = 4; d < W; d += step) {
        int x = cx - d / 2, y = cy - d / 2;
        if (x < 0 || y < 0) break;
        p.drawRectWH(x, y, d, d, fg);
    }
}

void pat_diagonals(int idx, uint16_t bg) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    p.drawLine(0, 0, W - 1, H - 1, p.C_YELLOW);
    p.drawLine(0, H - 1, W - 1, 0, p.C_CYAN);
    p.drawLine(W / 2, 0, W / 2, H - 1, p.C_GREEN);
    p.drawLine(0, H / 2, W - 1, H / 2, p.C_MAGENTA);
}

void pat_tiledCircles(int idx, uint16_t bg) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    int r = 8, step = 20;
    for (int y = step / 2; y < H; y += step)
        for (int x = step / 2; x < W; x += step) p.fillCircle(x, y, r, p.C_WHITE);
}

void test6_patternGallery() {
    Serial.println("\n[Test] Pattern gallery — per-primitive isolation");
    Serial.println("  Each pattern shown ~1.5s on D0 + D1 simultaneously:");
    Serial.println("    a) vertical hairlines step 4");
    Serial.println("    b) horizontal hairlines step 4");
    Serial.println("    c) pixel grid step 4");
    Serial.println("    d) pixel grid step 2 (dense)");
    Serial.println("    e) concentric rectangles");
    Serial.println("    f) diagonals + axis lines");
    Serial.println("    g) tiled filled circles");
    Serial.println();
    initBoth();

    struct Step {
        const char* label;
        void (*fn)(int);
    };
    Step steps[] = {
        {"a vert hairlines step 4",
         [](int i) { pat_hairlinesV(i, panel[0].C_WHITE, panel[0].C_BLACK, 4); }},
        {"b horz hairlines step 4",
         [](int i) { pat_hairlinesH(i, panel[0].C_WHITE, panel[0].C_BLACK, 4); }},
        {"c pixel grid step 4",
         [](int i) { pat_pixelGrid(i, panel[0].C_WHITE, panel[0].C_BLACK, 4); }},
        {"d pixel grid step 2 (dense)",
         [](int i) { pat_pixelGrid(i, panel[0].C_WHITE, panel[0].C_BLACK, 2); }},
        {"e concentric rectangles",
         [](int i) { pat_concentricRects(i, panel[0].C_WHITE, panel[0].C_BLACK, 8); }},
        {"f diagonals + axis lines",
         [](int i) { pat_diagonals(i, panel[0].C_BLACK); }},
        {"g tiled filled circles", [](int i) { pat_tiledCircles(i, panel[0].C_NAVY); }},
    };

    for (auto& s : steps) {
        Serial.printf("  -> %s\n", s.label);
        s.fn(0);
        s.fn(1);
        delay(1500);
    }
    Serial.println("  done");
}

// --- Code-path compare -----------------------------------------------------
// Same sparse dot grid drawn with several APIs. They should look
// identical; if any code path is broken on this controller the
// corresponding dots will be missing or wrong.

void cp_drawPixel(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int y = step / 2; y < H; y += step)
        for (int x = step / 2; x < W; x += step) p.drawPixel(x, y, fg);
}

void cp_fillRect1x1(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int y = step / 2; y < H; y += step)
        for (int x = step / 2; x < W; x += step) p.fillRect(x, y, 1, 1, fg);
}

void cp_fillRect2x2(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int y = step / 2; y < H; y += step)
        for (int x = step / 2; x < W; x += step) p.fillRect(x, y, 2, 2, fg);
}

void cp_vline1px(int idx, uint16_t fg, uint16_t bg, int step) {
    GC9D01_LTSM& p = panel[idx];
    p.fillScreen(bg);
    for (int y = step / 2; y < H; y += step)
        for (int x = step / 2; x < W; x += step) p.drawFastVLine(x, y, 1, fg);
}

void test7_codePathCompare() {
    Serial.println("\n[Test] Code-path compare — same dot pattern, 4 APIs");
    Serial.println("  Each pattern shown ~2s on D0 + D1:");
    Serial.println("    a) drawPixel              (fast pixel path)");
    Serial.println("    b) fillRect(x,y,1,1,c)    (1x1 setWindow path)");
    Serial.println("    c) fillRect(x,y,2,2,c)    (2x2 setWindow + 4 colors)");
    Serial.println("    d) drawFastVLine(x,y,1,c) (1px vert window)");
    Serial.println();
    initBoth();
    struct Step {
        const char* label;
        void (*fn)(int);
    };
    Step steps[] = {
        {"a drawPixel grid step 8",
         [](int i) { cp_drawPixel(i, panel[0].C_WHITE, panel[0].C_BLACK, 8); }},
        {"b fillRect(1,1) grid step 8",
         [](int i) { cp_fillRect1x1(i, panel[0].C_WHITE, panel[0].C_BLACK, 8); }},
        {"c fillRect(2,2) grid step 8",
         [](int i) { cp_fillRect2x2(i, panel[0].C_WHITE, panel[0].C_BLACK, 8); }},
        {"d drawFastVLine(1px) grid step 8",
         [](int i) { cp_vline1px(i, panel[0].C_WHITE, panel[0].C_BLACK, 8); }},
    };
    for (auto& s : steps) {
        Serial.printf("  -> %s\n", s.label);
        s.fn(0);
        s.fn(1);
        delay(2000);
    }
    Serial.println("  done");
}

// --- Sprite pipeline -------------------------------------------------------
// Renders shapes into a TFT_eSprite (the same path RobotEyes uses), byte-
// swaps the buffer to MSB-first, and pushes via drawBitmap16Data. If the
// production render path is healthy this will look crisp; smearing or
// wrong colors here points at byte-order or sprite-allocation issues.

void test8_spritePipeline() {
    Serial.println("\n[Test] Sprite pipeline (TFT_eSprite + byteSwap + drawBitmap16Data)");
    initBoth();
    if (!ensureSprite()) {
        Serial.println("  cannot run — sprite allocation failed (check heap/PSRAM)");
        return;
    }

    // Frame 1: solid background with a centered antialiased capsule —
    // exercises fillSmoothRoundRect (the same primitive RobotEyes uses
    // for the eye shape).
    sprite.fillSprite(TFT_BLACK);
    sprite.fillSmoothRoundRect(W / 2 - 50, H / 2 - 50, 100, 100, 50, TFT_CYAN, TFT_BLACK);
    sprite.fillSmoothCircle(W / 2, H / 2, 18, TFT_BLACK, TFT_CYAN);
    pushSpriteTo(0);
    pushSpriteTo(1);
    Serial.println("  frame 1: cyan capsule with black pupil — both eyes");
    delay(2500);

    // Frame 2: diagonal wide line — exercises drawWideLine (used for
    // the eyebrow).
    sprite.fillSprite(TFT_BLACK);
    sprite.drawWideLine(20, 30, W - 20, 30, 7, TFT_WHITE, TFT_BLACK);
    sprite.fillSmoothCircle(W / 2, H / 2, 40, TFT_GREEN, TFT_BLACK);
    pushSpriteTo(0);
    pushSpriteTo(1);
    Serial.println("  frame 2: white wide line + green circle");
    delay(2500);

    // Frame 3: color stress — confirms RGB565 byte order is right end-
    // to-end. If reds appear blue or vice-versa, byte-swap is wrong.
    sprite.fillSprite(TFT_BLACK);
    sprite.fillRect(0, 0, W, H / 3, TFT_RED);
    sprite.fillRect(0, H / 3, W, H / 3, TFT_GREEN);
    sprite.fillRect(0, 2 * H / 3, W, H / 3, TFT_BLUE);
    pushSpriteTo(0);
    pushSpriteTo(1);
    Serial.println("  frame 3: R/G/B horizontal bands — verify color order");
    delay(2500);
    Serial.println("  done");
}

// --- Misc ------------------------------------------------------------------

bool gInvert = true;
void toggleInvert() {
    gInvert = !gInvert;
    Serial.printf("\n[Action] TFTchangeInvertMode(%s) on both\n",
                  gInvert ? "true" : "false");
    panel[0].TFTchangeInvertMode(gInvert);
    panel[1].TFTchangeInvertMode(gInvert);
}

const char* rotName(GC9D01_LTSM::display_rotate_e r) {
    switch (r) {
        case GC9D01_LTSM::Degrees_0:
            return "0";
        case GC9D01_LTSM::Degrees_90:
            return "90";
        case GC9D01_LTSM::Degrees_180:
            return "180";
        case GC9D01_LTSM::Degrees_270:
            return "270";
    }
    return "?";
}

// Cycle the common rotation (applied to both panels) through
// 0/90/180/270. Both eyes share rotation = both eyes share tint;
// changing this knob shifts the tint together. Only Degrees_90 yields
// naturally upright eyes for tests 1–7 (which draw directly to the
// panels — the 180° physical opposition is not compensated for these
// tests); the 180° SW flip in pushSpriteTo handles test 8 at any
// rotation.
void cycleCommonRotation() {
    GC9D01_LTSM::display_rotate_e order[] = {
        GC9D01_LTSM::Degrees_0,
        GC9D01_LTSM::Degrees_90,
        GC9D01_LTSM::Degrees_180,
        GC9D01_LTSM::Degrees_270,
    };
    int n   = sizeof(order) / sizeof(order[0]);
    int cur = 0;
    for (int i = 0; i < n; ++i) {
        if (order[i] == commonRotation) {
            cur = i;
            break;
        }
    }
    commonRotation = order[(cur + 1) % n];
    Serial.printf("\n[Action] commonRotation = %s — re-initing both\n",
                  rotName(commonRotation));
    initBoth();
    drawIdPattern(0);
    drawIdPattern(1);
}

void printHelp() {
    Serial.println();
    Serial.println("=== GC9D01_LTSM dual-display debug ===");
    Serial.printf("  CS1=%d  CS2=%d  RST1=%d  RST2=%d\n", CS[0], CS[1], RST[0], RST[1]);
    Serial.printf("  MOSI=%d  SCLK=%d  DC=%d  W=%d  H=%d\n",
                  TFT_MOSI, TFT_SCLK, TFT_DC, W, H);
    Serial.printf("  Free heap: %u  PSRAM: %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    Serial.println();
    Serial.println("  1  Display 0 alone — color cycle + id");
    Serial.println("  2  Display 1 alone — color cycle + id");
    Serial.println("  3  Init both, draw distinct id patterns (CS isolation)");
    Serial.println("  4  CS-toggle stress (5s alternating fillScreen)");
    Serial.println("  5  Address-window quadrant test");
    Serial.println("  6  Pattern gallery (hairlines, pixel grid, rects, ...)");
    Serial.println("  7  Code-path compare (drawPixel/fillRect/vline)");
    Serial.println("  8  Sprite pipeline (TFT_eSprite -> byteSwap -> drawBitmap16Data)");
    Serial.println("  i  Toggle TFTchangeInvertMode on both");
    Serial.println("  t  Cycle common rotation 0/90/180/270 (tint knob)");
    Serial.println("  r  Re-init both panels");
    Serial.println("  ?  This help");
    Serial.println();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\nGC9D01_LTSM debug starting...");

    // Pin the SPI bus to our wiring before any panel touches it; the
    // arg-less SPI.begin() inside GC9D01_LTSM::TFTHWSPIInitialize will
    // then short-circuit on arduino-esp32's _spi guard.
    SPI.begin(TFT_SCLK, /*miso=*/-1, TFT_MOSI, /*ss=*/-1);

    printHelp();
}

void loop() {
    if (Serial.available() <= 0) return;
    int c = Serial.read();
    if (c < 0) return;
    switch ((char)c) {
        case '1':
            test1_displayAlone(0);
            break;
        case '2':
            test1_displayAlone(1);
            break;
        case '3':
            test2_csIsolation();
            break;
        case '4':
            test3_alternating();
            break;
        case '5':
            test4_addressWindow();
            break;
        case '6':
            test6_patternGallery();
            break;
        case '7':
            test7_codePathCompare();
            break;
        case '8':
            test8_spritePipeline();
            break;
        case 'i':
        case 'I':
            toggleInvert();
            break;
        case 't':
        case 'T':
            cycleCommonRotation();
            break;
        case 'r':
        case 'R':
            Serial.println("\n[Action] re-init both panels");
            initBoth();
            Serial.println("  done");
            break;
        case '?':
            printHelp();
            break;
        case '\r':
        case '\n':
            break;
        default:
            Serial.printf("Unknown key '%c'. '?' for help.\n", (char)c);
            break;
    }
}
