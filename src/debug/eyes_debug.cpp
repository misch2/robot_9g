// Standalone debug sketch for the dual-GC9D01 wiring. Excluded from the
// main robot build via build_src_filter — see platformio.ini's
// [env:esp32s3_eyes_debug].
//
// Goal: progressively isolate why RobotEyes shows mostly black on the
// real hardware. Each serial-key test exercises a different layer so we
// can pinpoint the failure (SPI? CS multiplexing? init sequence? address
// window?).
//
//   pio run -e esp32s3_eyes_debug -t upload -t monitor
//
// Then press '?' for the menu.

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace {

constexpr int CS[2]  = {PIN_TFT_CS1, PIN_TFT_CS2};
constexpr int RST[2] = {PIN_TFT_RST1, PIN_TFT_RST2};

TFT_eSPI tft;

void selectOnly(int idx) {
    digitalWrite(CS[idx], LOW);
    digitalWrite(CS[idx ^ 1], HIGH);
}

void deselectAll() {
    digitalWrite(CS[0], HIGH);
    digitalWrite(CS[1], HIGH);
}

void resetBoth(uint32_t holdMs = 50, uint32_t settleMs = 150) {
    digitalWrite(RST[0], LOW);
    digitalWrite(RST[1], LOW);
    delay(holdMs);
    digitalWrite(RST[0], HIGH);
    digitalWrite(RST[1], HIGH);
    delay(settleMs);
}

void initOne(int idx) {
    selectOnly(idx);
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);
}

// Rapid color cycle on whichever display is currently CS-selected.
void colorCycle(uint32_t totalMs) {
    static const uint16_t colors[] = {
        TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_MAGENTA, TFT_CYAN, TFT_WHITE,
    };
    constexpr int N = sizeof(colors) / sizeof(colors[0]);
    uint32_t per   = 500;
    uint32_t start = millis();
    int i          = 0;
    while (millis() - start < totalMs) {
        tft.fillScreen(colors[i % N]);
        Serial.printf("  color %d\n", i % N);
        delay(per);
        i++;
    }
    tft.fillScreen(TFT_BLACK);
}

// Draw a recognizable per-display pattern so we can tell which is which
// without recoloring globally.
void drawIdPattern(int idx) {
    uint16_t bg = (idx == 0) ? TFT_RED : TFT_BLUE;
    tft.fillScreen(bg);
    // Big digit: 1 or 2.
    tft.setTextColor(TFT_WHITE, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextSize(3);
    tft.drawString(idx == 0 ? "1" : "2", TFT_WIDTH / 2, TFT_HEIGHT / 2);
    // Border so partial-init shows up clearly.
    tft.drawRect(0, 0, TFT_WIDTH, TFT_HEIGHT, TFT_WHITE);
    tft.drawRect(4, 4, TFT_WIDTH - 8, TFT_HEIGHT - 8, TFT_WHITE);
}

// --- Tests ------------------------------------------------------------------

void test1_displayAlone(int idx) {
    Serial.printf("\n[Test] Display %d alone (other CS held high)\n", idx);
    resetBoth();
    selectOnly(idx);
    tft.init();
    tft.setRotation(TFT_ROTATION);
    Serial.println("  init() complete; cycling colors for 5s");
    colorCycle(5000);
    drawIdPattern(idx);
    deselectAll();
    Serial.println("  done — id pattern left on screen");
}

void test2_csIsolation() {
    Serial.println("\n[Test] CS isolation — init both, write distinct colors");
    resetBoth();
    initOne(0);
    initOne(1);

    // Display 0 → red with "1", Display 1 → blue with "2"
    selectOnly(0);
    drawIdPattern(0);
    selectOnly(1);
    drawIdPattern(1);
    deselectAll();
    Serial.println("  done — D0 should be RED with '1', D1 BLUE with '2'");
    Serial.println("  if both show the same → CS toggling is failing");
    Serial.println("  if one is blank → that display didn't init");
}

void test3_alternating() {
    Serial.println("\n[Test] Alternating writes for 5s — stress-test CS toggle");
    resetBoth();
    initOne(0);
    initOne(1);
    uint32_t start = millis();
    int frame      = 0;
    while (millis() - start < 5000) {
        // Rapid alternation: D0=red, D1=blue swapped each frame.
        bool a = frame & 1;
        selectOnly(0);
        tft.fillScreen(a ? TFT_RED : TFT_GREEN);
        selectOnly(1);
        tft.fillScreen(a ? TFT_BLUE : TFT_YELLOW);
        delay(200);
        frame++;
    }
    deselectAll();
    Serial.println("  done");
}

void test4_longResetTiming() {
    Serial.println("\n[Test] Long RST pulse (200ms low, 300ms settle)");
    resetBoth(200, 300);
    initOne(0);
    initOne(1);
    selectOnly(0);
    drawIdPattern(0);
    selectOnly(1);
    drawIdPattern(1);
    deselectAll();
    Serial.println("  done — same as test 2 but with longer reset timing");
}

void test5_addressWindow() {
    Serial.println("\n[Test] Address-window walk — colored quadrants");
    resetBoth();
    initOne(0);
    initOne(1);
    for (int idx = 0; idx < 2; ++idx) {
        selectOnly(idx);
        int w = TFT_WIDTH, h = TFT_HEIGHT;
        tft.fillScreen(TFT_BLACK);
        tft.fillRect(0, 0, w / 2, h / 2, TFT_RED);
        tft.fillRect(w / 2, 0, w / 2, h / 2, TFT_GREEN);
        tft.fillRect(0, h / 2, w / 2, h / 2, TFT_BLUE);
        tft.fillRect(w / 2, h / 2, w / 2, h / 2, TFT_WHITE);
        tft.drawLine(0, 0, w - 1, h - 1, TFT_YELLOW);
        tft.drawLine(0, h - 1, w - 1, 0, TFT_YELLOW);
    }
    deselectAll();
    Serial.println("  done — each display: 4 colored quadrants + diagonals");
    Serial.println("  if rectangles look shifted/wrapped → address-window math is wrong");
}

void test6_invertOn() {
    Serial.println("\n[Test] Invert ON — some GC9D01 panels boot inverted");
    resetBoth();
    initOne(0);
    initOne(1);
    for (int idx = 0; idx < 2; ++idx) {
        selectOnly(idx);
        tft.invertDisplay(true);
        drawIdPattern(idx);
    }
    deselectAll();
    Serial.println("  done — if previous tests were 'inverted colors' this fixes it");
    Serial.println("  press 'I' to toggle invert OFF and compare");
}

bool gInvert = true;
void toggleInvert() {
    gInvert = !gInvert;
    Serial.printf("\n[Action] invertDisplay(%s) on both\n", gInvert ? "true" : "false");
    selectOnly(0);
    tft.invertDisplay(gInvert);
    selectOnly(1);
    tft.invertDisplay(gInvert);
    deselectAll();
}

void printHelp() {
    Serial.println();
    Serial.println("=== GC9D01 dual-display debug ===");
    Serial.printf("  CS1=%d  CS2=%d  RST1=%d  RST2=%d\n", CS[0], CS[1], RST[0], RST[1]);
    Serial.printf("  MOSI=%d  SCLK=%d  DC=%d  TFT_WIDTH=%d  TFT_HEIGHT=%d\n",
                  TFT_MOSI, TFT_SCLK, TFT_DC, TFT_WIDTH, TFT_HEIGHT);
    Serial.printf("  Free heap: %u  PSRAM: %u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    Serial.println();
    Serial.println("  1  Display 0 alone (CS2 forced high) — color cycle + id");
    Serial.println("  2  Display 1 alone (CS1 forced high) — color cycle + id");
    Serial.println("  3  Init both, draw distinct id patterns (CS isolation)");
    Serial.println("  4  Stress-test CS toggle (5s alternating)");
    Serial.println("  5  Long RST pulse (200ms low) + id patterns");
    Serial.println("  6  Address-window quadrant test");
    Serial.println("  7  Invert ON + id patterns");
    Serial.println("  i  Toggle invertDisplay() on both");
    Serial.println("  r  Reset + re-init both");
    Serial.println("  ?  This help");
    Serial.println();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\nGC9D01 debug starting...");

    for (int i = 0; i < 2; ++i) {
        pinMode(CS[i], OUTPUT);
        pinMode(RST[i], OUTPUT);
        digitalWrite(CS[i], HIGH);
        digitalWrite(RST[i], LOW);
    }
    delay(20);

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
            test4_longResetTiming();
            break;
        case '6':
            test5_addressWindow();
            break;
        case '7':
            test6_invertOn();
            break;
        case 'i':
        case 'I':
            toggleInvert();
            break;
        case 'r':
        case 'R':
            Serial.println("\n[Action] reset + re-init both");
            resetBoth();
            initOne(0);
            initOne(1);
            deselectAll();
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
