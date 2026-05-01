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

There is a single environment, `esp32s3` (the default). No test runner is configured.

## Hardware wiring & pin policy

All pin assignments are passed as `-D` macros in `platformio.ini` (`build_flags`), not hardcoded in source — change them there, not in `src/`. Display pins use the `TFT_*` names that `TFT_eSPI` expects, so the lib is configured entirely via build flags (`USER_SETUP_LOADED=1`, `GC9A01_DRIVER=1`, etc.) — do not add a `User_Setup.h`.

Pin constraints documented in `platformio.ini` and that must be respected when adding peripherals:
- Reserved by flash/PSRAM: GPIO 26–37 (with Octal SPI variants, 35–37 are always memory).
- Strapping pins, use with care: GPIO 0, 3, 45, 46.
- ADC2 pins (GPIO 11–20) cannot be used for ADC while Wi‑Fi is active.

Current assignments: TFT on GPIO 8–12 (no MISO, no backlight pin); servos on GPIO 6, 7, 15, 16, 17, 18.

## Code architecture

The codebase is intentionally small. The layers, lowest first:

- `src/main.cpp` — Arduino `setup()`/`loop()`. Constructs a single `ServoManager`, a `ServoMotion` bound to it, and a `RobotMotion` bound to that; registers servos and calls both `begin()`s. `loop()` reads serial input (debug keys: direct per-servo control plus queued whole-robot moves — press `?` for the full list) and ticks `servoMotion.update()` then `robotMotion.update()`.
- `src/servo_manager.{h,cpp}` — drives hobby servos directly via the ESP32 LEDC peripheral (50 Hz, 16-bit resolution; ~0.3 µs pulse precision). One LEDC channel per servo, with the `ServoId` enum value used as both the array index *and* the LEDC channel number — so `addServo()` must be called once per `ServoId` in declaration order (the call logs and skips on mismatch), and the project is bounded to 8 servos on ESP32-S3 (LEDC channel limit). `begin()` attaches every registered servo and drives it to its neutral angle. `setAngle(ServoId, deg)` clamps to the per-servo `minAngle`/`maxAngle` before writing, and stores the clamped float in `ServoSettings::currentAngle` so `getCurrentAngle()` can return it. We use LEDC directly rather than the `ESP32Servo` library: `ESP32Servo` 3.x hung unreproducibly inside our setup path on this hardware/framework combo (the library worked in a minimal isolated sketch, so the cause was never pinned down — direct LEDC sidesteps the dependency entirely).
- `src/servo_motion.{h,cpp}` — time-based smooth movement on top of `ServoManager`. `moveTo(id, angle, durationMs, easing = EaseInOut)` schedules a movement; `update()` (called from `loop()`) interpolates and writes via `ServoManager::setAngle()`. Per-servo state is independent so multiple servos move concurrently. Re-issuing `moveTo` on a moving servo retargets from `ServoManager::getCurrentAngle()` (the last interpolated value) so the motion stays smooth instead of jumping. `Easing` enum: `Linear`, `EaseIn` (quadratic), `EaseOut` (quadratic), `EaseInOut` (piecewise quadratic). `isIdle()` and `isIdle(ServoId)` report whether motion is still in progress.
- `src/robot_motion.{h,cpp}` — whole-robot gait on top of `ServoMotion`. The mechanics: four leg servos lift via parallelograms; two body servos (Translation, Rotation) couple diagonal leg pairs (A = {RearLeft, FrontRight}, B = {FrontLeft, RearRight}). A half-step is lift-actuate-drop on one diagonal; the next half-step uses the other diagonal and the opposite body-servo extreme, producing net body motion. Public API: `step(int fullSteps)`, `rotate(float degrees)`, `crouch()`, `stand()` — all enqueue jobs into a fixed-size ring buffer (`kMaxJobs = 8`, drops on overflow with a serial log). `update()` advances a per-job state machine (`Lift → Actuate → Drop`, or single-shot `Pose`) that only progresses when `motion.isIdle()`. Tunables (leg up/down angles, body extremes, phase durations, `degreesPerHalfRotation`) live in the public `RobotMotion::config` (`Config` struct) — adjust at runtime, not by editing the header. `begin()` drives the standing pose immediately (synchronously issues `moveTo`s, doesn't wait).

`config.h` holds compile-time constants intrinsic to the firmware logic (default 500–2500 µs pulse range) and the `ServoId` enum that defines which logical servos exist. Anything tied to the specific board/wiring lives in `platformio.ini` build flags instead.

Libraries pinned in `platformio.ini`: `bodmer/TFT_eSPI`, `tzapu/WiFiManager`. WiFi/display code is not yet integrated into `main.cpp`. Servos are driven via the framework's built-in LEDC API — no servo library dependency.

## Style

`.clang-format` is Google-based with these deviations: 4-space indent, no column limit, `SortIncludes: false` (header order is intentional — keep `Arduino.h` first), aligned consecutive `#define`s and assignments, single-line `if` allowed without `else`.
