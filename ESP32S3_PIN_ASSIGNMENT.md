# ESP32-S3-DevKitC-1 Pin Assignment

Source of truth for all assignments below: `build_flags` in `platformio.ini` . Change pins there, not in `src/` .

## Legend

- ⏹️ - usable, no caveats
- ⚠️ - usable but has a constraint (strapping, ADC2+WiFi, USB, etc.)
- ❌ - must not be used (flash/PSRAM)
- **N/A** - pin does not exist on ESP32-S3

## Pin Map

The two `Assignment` columns correspond to the two production PlatformIO envs: `esp32s3` (direct LEDC servo driver) and `esp32s3_pca9685` (PCA9685 over I²C). Display pins are identical in both.

| GPIO  | Warn | `esp32s3` (LEDC)            | `esp32s3_pca9685`           | Notes                                                                |
| ----- | ---- | --------------------------- | --------------------------- | -------------------------------------------------------------------- |
| 0     | ⚠️   | -                           | -                           | Strapping (Boot button). Avoid.                                      |
| 1     |      | TFT_RST (mouth)             | TFT_RST (mouth)             | ADC1                                                                 |
| 2     |      | TFT_CS (mouth)              | TFT_CS (mouth)              | ADC1                                                                 |
| 3     | ⚠️   | -                           | -                           | Strapping. Avoid.                                                    |
| 4     |      | ⏹️                          | PIN_I2C_SDA (PCA9685)       | ADC1                                                                 |
| 5     |      | ⏹️                          | PIN_I2C_SCL (PCA9685)       | ADC1                                                                 |
| 6     |      | SERVO_1 (LEDC)              | PIN_PCA9685_OE              | PCA9685 ŌĒ (HIGH = outputs disabled, LOW = enabled)                  |
| 7     |      | SERVO_2 (LEDC)              | ⏹️                          |                                                                      |
| 8     |      | SERVO_7 (LEDC)              | ⏹️                          |                                                                      |
| 9     |      | PIN_EYES_SCLK               | PIN_EYES_SCLK               | ADC1                                                                 |
| 10    |      | PIN_EYES_MOSI               | PIN_EYES_MOSI               | ADC1                                                                 |
| 11    |      | PIN_EYES_DC                 | PIN_EYES_DC                 | ADC2 - unavailable for ADC when WiFi is up                           |
| 12    |      | PIN_TFT_CS1 (right eye, D0) | PIN_TFT_CS1 (right eye, D0) | ADC2 - unavailable for ADC when WiFi is up                           |
| 13    |      | PIN_TFT_RST1 (right eye)    | PIN_TFT_RST1 (right eye)    | ADC2 - unavailable for ADC when WiFi is up                           |
| 14    |      | PIN_TFT_CS2 (left eye, D1)  | PIN_TFT_CS2 (left eye, D1)  | ADC2 - unavailable for ADC when WiFi is up                           |
| 15    |      | SERVO_3 (LEDC)              | ⏹️                          | ADC2 - unavailable for ADC when WiFi is up                           |
| 16    |      | SERVO_4 (LEDC)              | ⏹️                          | ADC2 - unavailable for ADC when WiFi is up                           |
| 17    |      | SERVO_5 (LEDC)              | ⏹️                          | ADC2 - unavailable for ADC when WiFi is up                           |
| 18    |      | SERVO_6 (LEDC)              | ⏹️                          | ADC2 - unavailable for ADC when WiFi is up                           |
| 19    | ⚠️   | -                           | -                           | USB D− (used by native USB-Serial when `ARDUINO_USB_MODE=1` ). ADC2. |
| 20    | ⚠️   | -                           | -                           | USB D+. ADC2. Conflicts with native USB if active.                   |
| 21    |      | PIN_TFT_RST2 (left eye)     | PIN_TFT_RST2 (left eye)     |                                                                      |
| 22-25 | N/A  | -                           | -                           | Pin does not exist on ESP32-S3                                       |
| 26-37 | ❌   | -                           | -                           | SPI flash / PSRAM                                                    |
| 38    |      | TFT_DC (mouth)              | TFT_DC (mouth)              |                                                                      |
| 39    |      | TFT_MOSI (mouth)            | TFT_MOSI (mouth)            |                                                                      |
| 40    |      | TFT_SCLK (mouth)            | TFT_SCLK (mouth)            |                                                                      |
| 41    | ⏹️   | ⏹️                          | ⏹️                          |                                                                      |
| 42    |      | SERVO_8 (LEDC)              | ⏹️                          |                                                                      |
| 43    | ⚠️   | -                           | -                           | UART0 TX (default Serial when CDC-on-boot is off)                    |
| 44    | ⚠️   | -                           | -                           | UART0 RX (default Serial when CDC-on-boot is off)                    |
| 45    | ⚠️   | -                           | -                           | Strapping (VDD_SPI). Avoid.                                          |
| 46    | ⚠️   | -                           | -                           | Strapping. Avoid.                                                    |
| 47    |      | ⏹️                          | ⏹️                          |                                                                      |
| 48    | ⚠️   | ⏹️                          | ⏹️                          | On-board WS2812 RGB LED on most DevKitC-1 boards                     |

## Summary of free pins

### Unconditionally free

- `esp32s3` (LEDC): GPIO 4, 5, 41, 47, 48
- `esp32s3_pca9685`: GPIO 7, 8, 15, 16, 17, 18, 41, 42, 47, 48 (servo GPIOs are released because every channel is on the PCA9685; GPIO 6 is repurposed as the PCA9685 ŌĒ line)

### Free with caveats

- GPIO 0, 3, 45, 46 - strapping pins; usable as outputs after boot but avoid for inputs that could be driven during reset
- GPIO 19 - USB D− (avoid if native USB used)
- GPIO 20 - USB D+ (avoid if native USB used)
- GPIO 43, 44 - free if you give up the UART0 Serial console
- GPIO 48 - usually wired to the board's on-board RGB LED

### ADC notes

- ADC1: GPIO 1–10 - works with WiFi
- ADC2: GPIO 11–20 - **cannot be used for ADC while WiFi is active**

## Current assignment recap

**Servos** —

- `esp32s3` (LEDC channels 0–7): GPIO 6, 7, 15, 16, 17, 18, 8, 42
- `esp32s3_pca9685` (I²C to PCA9685 @ `0x40`, 16 channels): SDA=4, SCL=5, ŌĒ=6 (drive HIGH to disable all outputs, LOW to enable). The ESP32 GPIOs `PIN_SERVO_1..8` are still passed in as build flags but ignored at runtime.

**Mouth** — GC9A01 240×240 on HSPI via TFT_eSPI (`-DUSE_HSPI_PORT=1`):

- SCLK=40, MOSI=39, DC=38
- CS=2, RST=1

**Eyes** — dual GC9D01 160×160 on the default `SPI` (FSPI) via GC9D01_LTSM:

- Shared bus: SCLK=9, MOSI=10, DC=11
- Right eye (D0): CS1=12, RST1=13
- Left eye (D1): CS2=14, RST2=21

The `esp32s3_eyes_debug` env reuses the eye pins for its standalone sketch; the mouth pins are unused there.
