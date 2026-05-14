# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32-S3 firmware (PlatformIO + Arduino framework) for a small robot driving up to 6 hobby servos and a GC9A01 240x240 round TFT display. Target board is `esp32-s3-devkitc-1` with 16 MB flash and 8 MB PSRAM (Octal SPI / `qio_opi`).

## Build / Flash / Monitor

PlatformIO CLI is the canonical build interface (the VS Code extension wraps the same commands):

- Build: `pio run`
- Upload (flash over USB): `pio run -t upload`
- Serial monitor: `pio device monitor` (115200 baud)
- Build + upload + monitor: `pio run -t upload -t monitor`
- Clean: `pio run -t clean`

Environments:
- `esp32s3` (default) — production firmware. Servos driven via a PCA9685 I²C servo board (up to 16 channels). I²C pins and chip address are set as `-D` macros in `build_flags`.
- `esp32s3_eyes_debug` — standalone dual-eye wiring test sketch (see Display layer below).

No test runner is configured.

## Hardware wiring & pin policy

All pin assignments are passed as `-D` macros in `platformio.ini` (`build_flags`), not hardcoded in source — change them there, not in `src/`. Display pins use the `TFT_*` names that `TFT_eSPI` expects, so the lib is configured entirely via build flags (`USER_SETUP_LOADED=1`, `GC9A01_DRIVER=1`, etc.) — do not add a `User_Setup.h`.

Pin constraints documented in `platformio.ini` and that must be respected when adding peripherals:
- Reserved by flash/PSRAM: GPIO 26–37 (with Octal SPI variants, 35–37 are always memory).
- Strapping pins, use with care: GPIO 0, 3, 45, 46.
- ADC2 pins (GPIO 11–20) cannot be used for ADC while Wi‑Fi is active.

Current assignments: TFT on GPIO 8–12 (no MISO, no backlight pin); PCA9685 servo board on I²C (SDA=4, SCL=5).

## Code architecture

The codebase is intentionally small. The layers, lowest first:

- `src/main.cpp` — Arduino `setup()`/`loop()`. Constructs a single `ServoManager`, a `ServoMotion` bound to it, and a `RobotMotion` bound to that; registers servos and calls both `begin()`s. `loop()` reads serial input (debug keys: direct per-servo control plus queued whole-robot moves — press `?` for the full list) and ticks `servoMotion.update()` then `robotMotion.update()`.
- `src/servo.{h,cpp}` — single-servo driver over a PCA9685. The header declares the public `Servo` API plus `kMaxServos = 16` and `servoBackendBegin()`; the `.cpp` owns the shared `Adafruit_PWMServoDriver` as a file-scope pointer, initialized by `servoBackendBegin()` and used by every `Servo`. 50 Hz, 12-bit resolution (~4.88 µs — below the ~1–3° backlash of a 9g servo, so the chip is not the precision-limiting factor). `attach()`/`detach()`/`setAngle(float)` (clamped to `minAngle`/`maxAngle`, clamped float kept for `getCurrentAngle()`). Not RAII — `attach`/`detach` are explicit so instances live in a `std::vector` inside `ServoManager`.
- `src/servo_manager.{h,cpp}` — owns the group of registered `Servo`s and dispatches by `ServoId`. The `ServoId` enum value is used as both the array index *and* the PCA9685 channel number, so servos must be registered once per `ServoId` in declaration order. A `static_assert` bounds `ServoId::_Count` to `kMaxServos` (16). `begin()` calls `servoBackendBegin()` once, attaches every registered servo, and drives it to its neutral angle. `setAngle(ServoId, deg)` and `getCurrentAngle(ServoId)` are thin lookups onto the underlying `Servo`.
- `src/servo_motion.{h,cpp}` — time-based smooth movement on top of `ServoManager`. `moveTo(id, angle, durationMs, easing = EaseInOut)` schedules a movement; `update()` (called from `loop()`) interpolates and writes via `ServoManager::setAngle()`. Per-servo state is independent so multiple servos move concurrently. Re-issuing `moveTo` on a moving servo retargets from `ServoManager::getCurrentAngle()` (the last interpolated value) so the motion stays smooth instead of jumping. `Easing` enum: `Linear`, `EaseIn` (quadratic), `EaseOut` (quadratic), `EaseInOut` (piecewise quadratic). `isIdle()` and `isIdle(ServoId)` report whether motion is still in progress.
- `src/robot_motion.{h,cpp}` — whole-robot orchestrator on top of `ServoMotion`. Owns the public API (`step`, `rotate`, `sit`, `crouch`, `stand`, `dance`), a fixed-size job ring buffer (`kMaxJobs = 8`, drops on overflow with a serial log), and the public `RobotConfig config` (tunables: leg up/down fractions, body extremes, phase durations, `degreesPerRotation` — adjust at runtime). `update()` ticks whichever mover is currently active; when the mover signals done, the next queued job is dispatched to the appropriate mover. `begin()` dispatches a Stand pose job. `isIdle()` is true when no mover is active and the queue is empty.
- `src/robot_movers/` — the three independent state machines that `RobotMotion` dispatches into, one file pair per mover. Each owns its own timing/phase state and exposes `start(...)` and `bool update(now)` (returns true when fully done).
  - `robot_config.h` — the `RobotConfig` struct shared by all three movers, plus `fractionBalanced` / `fractionMax` / `fractionMin` shorthands.
  - `half_step_mover.{h,cpp}` — Walk: lift one diagonal pair, drive Translation to one extreme, drop — with overlapping schedule: actuate begins `bodyLeadInMs` after the lift, drop is timed so legs touch down `bodySettleMs` after the body finishes. Diagonal alternation persists across jobs. A regular Walk job ends with Translation at one extreme (`needsRecenter()` is true); recentering is orchestrator-driven via `startRecenter()`, so `RobotMotion` skips it when the next queued job is a same-direction Walk and consecutive forward presses chain without a wasted recenter between them. Mechanics reminder: diagonal A = {RearLeft, FrontRight}, diagonal B = {FrontLeft, RearRight}; the Translation servo moves A and B in opposite mechanical directions, so the alternation between half-steps is what produces net body motion.
  - `rotation_mover.{h,cpp}` — Rotate: the chassis is rigidly attached to one diagonal (the "body diagonal"); the Rotation servo couples it to the other diagonal and is mechanically one-sided, so its angle alternates between 0 and `+max` — never `−max`. Each rotate job is N full steps; one full step is two half-steps: (1) lift one diagonal and drive servo `0`→`+max`, (2) lift the other diagonal and drive servo `+max`→`0`. Body yaw only happens during the half-step where the body diagonal is airborne; the other half-step is purely realigning the loose legs. Net rotation per full step is `degreesPerRotation`. Direction is selected by which diagonal lifts first: `sign>0` lifts diagA first, `sign<0` lifts diagB first. Servo starts and ends each job at 0, so no state persists across jobs. Within a half-step the same `bodyLeadInMs` / `bodySettleMs` overlap as `half_step_mover` applies. Unlike `half_step_mover`, the half-steps themselves overlap: each half-step ends as soon as its body has settled, and the leg drop continues into the next half-step's lift on the opposite diagonal — both diagonals are briefly airborne at the boundary. The final half-step of a job waits for legs to land before signalling done.
  - `dance_mover.{h,cpp}` — lift each leg clockwise, hold `danceHoldMs`, drop.
  - `pose_mover.{h,cpp}` — drive every relevant servo to one of the static `Pose::Stand|Crouch|Sit` layouts (Crouch leaves Translation/Rotation alone), done when `motion.isIdle()`.

Display layer — three panels driven simultaneously in the production env. Two SPI buses to avoid contention:
- **HSPI** (via TFT_eSPI's `-DUSE_HSPI_PORT=1`): the mouth panel.
- **FSPI** (the default Arduino `SPI` object): the two eye panels, sharing the bus + DC line with per-panel CS/RST.

The two display classes are kept in sync from `main.cpp`, which forwards every `setExpression()` / `cycleExpression()` to both. `RobotEyes::Expression` is a `using` alias of `RobotFace::Expression`, so the enum is a single type and main.cpp passes one value to both classes:
- `src/robot_face.{h,cpp}` — GC9A01 240×240 round panel. Currently mouth-only (the file is intentionally minimal so a nose can be added later); `update()` redraws only on expression change.
- `src/robot_eyes.{h,cpp}` — two GC9D01 160×160 panels. Renders into a single `TFT_eSprite` (160×160 RGB565), then bulk-pushes via `SPI.writeBytes` after one `setAddrWindow(0,0,W-1,H-1)` per frame — bypassing `GC9D01_LTSM::drawBitmap16Data` (per-row addr-window + per-byte SPI). Both panels stay at MADCTL `Degrees_0` (matching tint); panel 1 is mounted physically 180° opposed and gets a 180° in-place buffer flip in software before push.
- `src/debug/eyes_debug.cpp` — standalone sketch (`pio run -e esp32s3_eyes_debug`) for the dual-eye wiring; mirrors the production sprite/SPI path but with menu-driven test patterns and a tint-cycler. Uses the same `PIN_EYES_*` macros as `robot_eyes.cpp`.

Orientation contract for `RobotEyes`: with MADCTL `Degrees_0` the GC9D01 displays the buffer rotated 90° CCW visually. Two paths cancel that out:
- Procedurally drawn sprites (eye capsule, brow, …): drawn in upright sprite coordinates, then `pushSprite()` applies an in-place 90° CW rotation before SPI burst (default `alreadyRotated=false`).
- BMP assets in `src/assets/eye*_data.h`: `tools/bmp_to_header.py` pre-rotates 90° CW at conversion time, so `showTestImage()` and any other BMP path calls `pushSprite(idx, /*alreadyRotated=*/true)` to skip the runtime rotation.
After this, the per-panel 180° flip for the physically opposed panel still applies on top.

`config.h` holds compile-time constants intrinsic to the firmware logic (default 500–2500 µs pulse range) and the `ServoId` enum that defines which logical servos exist. Anything tied to the specific board/wiring lives in `platformio.ini` build flags instead.

Libraries pinned in `platformio.ini`: `bodmer/TFT_eSPI`, `tzapu/WiFiManager`, `adafruit/Adafruit PWM Servo Driver Library` (PCA9685), `adafruit/Adafruit INA219`. WiFi/display code is not yet integrated into `main.cpp`.

## Style

`.clang-format` is Google-based with these deviations: 4-space indent, no column limit, `SortIncludes: false` (header order is intentional — keep `Arduino.h` first), aligned consecutive `#define`s and assignments, single-line `if` allowed without `else`.

## Measurements and statistics:

40 MHz SPI:

```
[Test] FPS benchmark
  Phase A (pure SPI push, 51200-byte frames): running 3s...
  Phase B (sprite render + push): running 3s...
  Phase A: 127 frames in 3000 ms = 42.3 fps  (~4.3 MB/s SPI)
  Phase B: 111 frames in 3004 ms = 37.0 fps  (sprite render included)
  SPI clock = 40000000 Hz; theoretical pure-SPI ceiling for 2x 51200-byte frames = 48.8 fps
```

60 MHz SPI:
```
[Test] FPS benchmark
  Phase A (pure SPI push, 51200-byte frames): running 3s...
  Phase B (sprite render + push): running 3s...
  Phase A: 224 frames in 3002 ms = 74.6 fps  (~7.6 MB/s SPI)
  Phase B: 179 frames in 3016 ms = 59.4 fps  (sprite render included)
  SPI clock = 80000000 Hz; theoretical pure-SPI ceiling for 2x 51200-byte frames = 97.7 fps
```