// Standalone debug sketch for the dual-GC9D01 wiring. Excluded from the
// main robot build via build_src_filter — see platformio.ini's
// [env:esp32s3_eyes_debug].
//
// Mirrors the production render path used by RobotEyes — sprite/bitmap
// only, no per-pixel panel APIs:
//   • GC9D01_LTSM owns each panel only for init + setAddrWindow.
//   • TFT_eSPI is included only for TFT_eSprite — the offscreen 160×160
//     RGB565 buffer where antialiased shapes are drawn — and is never
//     init()'d as a panel itself.
//   • Each frame: render into sprite → setAddrWindow once → SPI bulk
//     write the whole buffer (FIFO + DMA), bypassing the driver's
//     per-row / per-byte SPI loops.
//
//   pio run -e esp32s3_eyes_debug -t upload -t monitor
//
// Then press '?' for the menu.

#include <Arduino.h>
#include <GC9D01_LTSM.hpp>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "../assets/eye1_data.h"
#include "../assets/eye2_data.h"
#include "../assets/eye3_data.h"

namespace {

constexpr int CS[2]  = {PIN_TFT_CS1, PIN_TFT_CS2};
constexpr int RST[2] = {PIN_TFT_RST1, PIN_TFT_RST2};

constexpr int W = TFT_WIDTH;
constexpr int H = TFT_HEIGHT;

constexpr uint32_t kSpiFreqHz = 80000000;

// Both panels share one MADCTL rotation so they share the same tint
// (cheap GC9D01 modules show subtle color shifts depending on MX/MY/ML
// bits — same bits both sides, same shift). The panels are mounted 180°
// opposed, and pushSpriteTo() compensates for that with an in-place
// rotate of the buffer for kFlipPanel before the SPI burst. The 't' key
// cycles this rotation through all four values so you can pick whichever
// tint you like.
GC9D01_LTSM::display_rotate_e commonRotation = GC9D01_LTSM::Degrees_0;

// Which panel gets 180°-flipped in software. Mirrors RobotEyes'
// kFlipPanel.
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
    // pushImage() copies pixels verbatim unless swap is enabled, but the rest
    // of the sprite API stores byte-swapped (see pushSpriteTo() note). Without
    // this, host-order RGB565 from headers like assets/eye1_data.h ends up
    // byte-flipped on the panel — random-palette look.
    sprite.setSwapBytes(true);
    spriteAllocated = true;
    return true;
}

// Bulk-push raw MSB-first RGB565 bytes to one panel, bypassing
// drawBitmap16Data. The driver's drawBitmap16Data re-issues setAddrWindow
// per row and then drains the row via spiWriteDataBuffer, which itself
// calls SPI.transfer(b) byte-by-byte — order(s) of magnitude slower than
// the SPI clock can sustain. We use the public setAddrWindow once for
// the whole frame, then drive the SPI transaction directly with
// SPI.writeBytes (FIFO + DMA on ESP32-S3). DC stays HIGH for the entire
// data burst because no command interrupts it.
void pushFast(int idx, const uint8_t* buf, size_t bytes) {
    panel[idx].setAddrWindow(0, 0, W - 1, H - 1);  // sets window + RAMWR
    SPI.beginTransaction(SPISettings(kSpiFreqHz, MSBFIRST, SPI_MODE0));
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(CS[idx], LOW);
    SPI.writeBytes(buf, bytes);
    digitalWrite(CS[idx], HIGH);
    SPI.endTransaction();
}

// Push the sprite buffer to one panel — same path RobotEyes uses in
// production. TFT_eSprite at 16bpp pre-byte-swaps every write before
// storing (Sprite.cpp:1642), so the buffer is already MSB-first in
// memory and goes straight to the SPI bus. The kFlipPanel index gets a
// 180° in-place rotation so its visual orientation matches the other
// eye despite the physical mounting being 180° opposed.
void pushSpriteTo(int idx) {
    uint16_t* buf = static_cast<uint16_t*>(sprite.getPointer());
    if (!buf) return;
    if (idx == kFlipPanel) {
        rotate180InPlace(buf, (size_t)W * H);
    }
    pushFast(idx, reinterpret_cast<const uint8_t*>(buf), (size_t)W * H * 2);
}

// Sprite-rendered ID frame — used after re-init to confirm both panels
// are alive and to expose the per-rotation tint. Panel 0 → red w/ a
// black disc; panel 1 → blue w/ a black disc. Goes through pushSpriteTo
// so it also exercises the kFlipPanel rotation path.
void drawSpriteIdPattern() {
    sprite.fillSprite(TFT_RED);
    sprite.fillSmoothCircle(W / 2, H / 2, 40, TFT_BLACK, TFT_RED);
    pushSpriteTo(0);
    sprite.fillSprite(TFT_BLUE);
    sprite.fillSmoothCircle(W / 2, H / 2, 40, TFT_BLACK, TFT_BLUE);
    pushSpriteTo(1);
}

// --- Tests ------------------------------------------------------------------

// --- Sprite pipeline -------------------------------------------------------
// Renders shapes into a TFT_eSprite (the same path RobotEyes uses) and
// pushes via pushSpriteTo's bulk-SPI fast path. Smearing or wrong colors
// here points at byte-order or sprite-allocation issues.

void testSpritePipeline() {
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

// --- Static image cycler ---------------------------------------------------
// Cycles through the bundled eye bitmaps on both panels via the same sprite
// pipeline. Panel 1 receives a V-flipped copy of the source so that, once
// pushSpriteTo(1) applies its physical-mount 180° compensation, the two
// eyes appear mirrored on screen (the source bitmap is itself pre-rotated
// 90° CW by tools/bmp_to_header.py, so a V-flip in source space lands as
// an H-flip in viewer space). '2' toggles the cycle on/off; loop() ticks
// tickEyeCycle() to advance frames.

struct EyeFrame {
    const uint16_t* pixels;
    int w;
    int h;
    const char* name;
};

constexpr EyeFrame kEyeFrames[] = {
    {assets::kEye1Pixels, assets::kEye1Width, assets::kEye1Height, "eye1"},
    {assets::kEye2Pixels, assets::kEye2Width, assets::kEye2Height, "eye2"},
    {assets::kEye3Pixels, assets::kEye3Width, assets::kEye3Height, "eye3"},
};
constexpr int      kEyeFrameCount      = sizeof(kEyeFrames) / sizeof(kEyeFrames[0]);
constexpr uint32_t kEyeCycleIntervalMs = 5000;

bool     eyeCycling     = false;
int      eyeCycleIdx    = 0;
uint32_t eyeCycleNextMs = 0;

void renderEyeFrame(int idx) {
    const EyeFrame& f = kEyeFrames[idx];
    int ox            = (W - f.w) / 2;
    int oy            = (H - f.h) / 2;

    sprite.fillSprite(TFT_BLACK);
    sprite.pushImage(ox, oy, f.w, f.h, f.pixels);
    pushSpriteTo(0);

    sprite.fillSprite(TFT_BLACK);
    for (int y = 0; y < f.h; ++y) {
        const uint16_t* src = f.pixels + (f.h - 1 - y) * f.w;
        sprite.pushImage(ox, oy + y, f.w, 1, src);
    }
    pushSpriteTo(1);
}

void testEyeCycle() {
    if (eyeCycling) {
        eyeCycling = false;
        Serial.println("\n[Action] eye cycle stopped");
        return;
    }
    Serial.println("\n[Test] cycle eye bitmaps every 5s (press '2' to stop)");
    initBoth();
    if (!ensureSprite()) {
        Serial.println("  cannot run -- sprite allocation failed");
        return;
    }
    eyeCycling  = true;
    eyeCycleIdx = 0;
    Serial.printf("  -> %s\n", kEyeFrames[eyeCycleIdx].name);
    renderEyeFrame(eyeCycleIdx);
    eyeCycleNextMs = millis() + kEyeCycleIntervalMs;
}

void tickEyeCycle() {
    if (!eyeCycling) return;
    if ((int32_t)(millis() - eyeCycleNextMs) < 0) return;
    eyeCycleIdx = (eyeCycleIdx + 1) % kEyeFrameCount;
    Serial.printf("  -> %s\n", kEyeFrames[eyeCycleIdx].name);
    renderEyeFrame(eyeCycleIdx);
    eyeCycleNextMs = millis() + kEyeCycleIntervalMs;
}

// --- FPS benchmark ---------------------------------------------------------
// Two phases, ~3s each:
//   A) Pure SPI throughput: alternate two pre-rendered sprite buffers and push
//      to both panels. Measures the cost of two drawBitmap-equivalent SPI
//      bursts per frame plus the rotate180 on D1.
//   B) Full pipeline: per frame, fillSprite + draw a small animated shape +
//      push to both panels. Includes TFT_eSprite render cost.
// Reports frames/second for each (one frame = both panels updated).

void testFpsBench() {
    Serial.println("\n[Test] FPS benchmark");
    initBoth();
    if (!ensureSprite()) {
        Serial.println("  cannot run -- sprite allocation failed");
        return;
    }
    constexpr uint32_t kPhaseMs = 3000;

    // --- Phase A: pure SPI push throughput.
    // Build two static buffers (DMA-capable internal RAM) so we alternate
    // content without touching the sprite. Pushing identical buffers each
    // frame would be honest SPI throughput, but alternating proves the
    // panel actually latches new pixels.
    constexpr size_t kFrameBytes = (size_t)W * H * 2;
    uint8_t* bufA = (uint8_t*)heap_caps_malloc(kFrameBytes, MALLOC_CAP_DMA);
    uint8_t* bufB = (uint8_t*)heap_caps_malloc(kFrameBytes, MALLOC_CAP_DMA);
    if (!bufA || !bufB) {
        Serial.println("  cannot run -- DMA buffer alloc failed");
        free(bufA);
        free(bufB);
        return;
    }
    // Fill with two distinguishable patterns (MSB-first RGB565).
    for (size_t i = 0; i < kFrameBytes; i += 2) {
        bufA[i]     = 0xF8;  // red high byte
        bufA[i + 1] = 0x00;
        bufB[i]     = 0x00;
        bufB[i + 1] = 0x1F;  // blue low byte
    }
    Serial.printf("  Phase A (pure SPI push, %u-byte frames): running %lus...\n",
                  (unsigned)kFrameBytes, (unsigned long)(kPhaseMs / 1000));
    uint32_t startA  = millis();
    uint32_t framesA = 0;
    while (millis() - startA < kPhaseMs) {
        const uint8_t* buf = (framesA & 1) ? bufB : bufA;
        pushFast(0, buf, kFrameBytes);
        pushFast(1, buf, kFrameBytes);
        ++framesA;
    }
    uint32_t elapsedA = millis() - startA;
    free(bufA);
    free(bufB);

    // --- Phase B: full sprite pipeline.
    Serial.printf("  Phase B (sprite render + push): running %lus...\n",
                  (unsigned long)(kPhaseMs / 1000));
    uint32_t startB  = millis();
    uint32_t framesB = 0;
    while (millis() - startB < kPhaseMs) {
        sprite.fillSprite(TFT_BLACK);
        // Animated circle so the render isn't trivially constant.
        int cx = W / 2 + (int)((framesB * 3) % W) - W / 2;
        sprite.fillSmoothCircle(cx, H / 2, 30, TFT_GREEN, TFT_BLACK);
        pushSpriteTo(0);
        pushSpriteTo(1);
        ++framesB;
    }
    uint32_t elapsedB = millis() - startB;

    float fpsA       = framesA * 1000.0f / elapsedA;
    float fpsB       = framesB * 1000.0f / elapsedB;
    float bytesPerSA = framesA * 2.0f * kFrameBytes * 1000.0f / elapsedA;
    Serial.printf("  Phase A: %lu frames in %lu ms = %.1f fps  (~%.1f MB/s SPI)\n",
                  (unsigned long)framesA, (unsigned long)elapsedA, fpsA, bytesPerSA / 1e6f);
    Serial.printf("  Phase B: %lu frames in %lu ms = %.1f fps  (sprite render included)\n",
                  (unsigned long)framesB, (unsigned long)elapsedB, fpsB);
    Serial.printf("  SPI clock = %lu Hz; theoretical pure-SPI ceiling for 2x %u-byte frames = %.1f fps\n",
                  (unsigned long)kSpiFreqHz, (unsigned)kFrameBytes,
                  (float)kSpiFreqHz / (8.0f * 2.0f * kFrameBytes));
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
// changing this knob shifts the tint together. The 180° SW flip in
// pushSpriteTo keeps both eyes' visual orientation matching at any
// rotation, so this is purely a tint knob — pick whichever you like.
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
    if (ensureSprite()) {
        drawSpriteIdPattern();
    }
}

void printHelp() {
    Serial.println();
    Serial.println("=== GC9D01_LTSM dual-display debug ===");
    Serial.printf("  CS1=%d  CS2=%d  RST1=%d  RST2=%d\n", CS[0], CS[1], RST[0], RST[1]);
    Serial.printf("  MOSI=%d  SCLK=%d  DC=%d  W=%d  H=%d  SPI=%lu Hz\n",
                  TFT_MOSI, TFT_SCLK, TFT_DC, W, H, (unsigned long)kSpiFreqHz);
    Serial.printf("  Free heap: %u  PSRAM: %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    Serial.println();
    Serial.println("  1  Sprite pipeline (capsule / wide line / R-G-B bands)");
    Serial.println("  2  Cycle eye bitmaps every 5s (toggle; right eye mirrored)");
    Serial.println("  3  FPS benchmark (pure SPI push + full sprite pipeline)");
    Serial.println("  i  Toggle TFTchangeInvertMode on both");
    Serial.println("  t  Cycle common rotation 0/90/180/270 (tint knob)");
    Serial.println("  r  Re-init both panels + push sprite ID pattern");
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
    tickEyeCycle();
    if (Serial.available() <= 0) return;
    int c = Serial.read();
    if (c < 0) return;
    switch ((char)c) {
        case '1':
            testSpritePipeline();
            break;
        case '2':
            testEyeCycle();
            break;
        case '3':
            testFpsBench();
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
            if (ensureSprite()) {
                drawSpriteIdPattern();
            }
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
