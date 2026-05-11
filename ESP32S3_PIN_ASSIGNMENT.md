# ESP32-S3-DevKitC-1 Pin Assignment

Source of truth for all assignments below: `build_flags` in `platformio.ini` . Change pins there, not in `src/` .

## Legend

* **FREE** - usable, no caveats
* ⚠️ - usable but has a constraint (strapping, ADC2+WiFi, USB, etc.)
* ❌ - must not be used (flash/PSRAM)
* **N/A** - pin does not exist on ESP32-S3

## Pin Map

| GPIO | Status | Assignment (esp32s3 / face) | Assignment (esp32s3_eyes) | Notes                                                                |
| ---- | ------ | --------------------------- | ------------------------- | -------------------------------------------------------------------- |
| 0    | ⚠️     | -                           | -                         | Strapping (Boot button). Avoid.                                      |
| 1    | ✅     | TFT_RST                     | -                         | ADC1                                                                 |
| 2    | ✅     | TFT_CS                      | -                         | ADC1                                                                 |
| 3    | ⚠️     | -                           | -                         | Strapping. Avoid.                                                    |
| 4    | FREE   | -                           | -                         | ADC1                                                                 |
| 5    | FREE   | -                           | -                         | ADC1                                                                 |
| 6    | ✅     | SERVO_1 (LEDC)              | SERVO_1 (LEDC)            |                                                                      |
| 7    | ✅     | SERVO_2 (LEDC)              | SERVO_2 (LEDC)            |                                                                      |
| 8    | ✅     | SERVO_7 (LEDC)              | SERVO_7 (LEDC)            |                                                                      |
| 9    | ✅     | -                           | TFT_SCLK                  | ADC1                                                                 |
| 10   | ✅     | -                           | TFT_MOSI                  | ADC1                                                                 |
| 11   | ✅     | -                           | TFT_DC                    | ADC2 - unavailable for ADC when WiFi is up                           |
| 12   | ✅     | -                           | TFT_CS1 (right eye, D0)   | ADC2 - unavailable for ADC when WiFi is up                           |
| 13   | ✅     | -                           | TFT_RST1 (right eye)      | ADC2                                                                 |
| 14   | ✅     | -                           | TFT_CS2 (left eye, D1)    | ADC2 (no ADC with WiFi)                                              |
| 15   | ✅     | SERVO_3 (LEDC)              | SERVO_3 (LEDC)            | ADC2                                                                 |
| 16   | ✅     | SERVO_4 (LEDC)              | SERVO_4 (LEDC)            | ADC2                                                                 |
| 17   | ✅     | SERVO_5 (LEDC)              | SERVO_5 (LEDC)            | ADC2                                                                 |
| 18   | ✅     | SERVO_6 (LEDC)              | SERVO_6 (LEDC)            | ADC2                                                                 |
| 19   | ⚠️     | -                           | -                         | USB D− (used by native USB-Serial when `ARDUINO_USB_MODE=1` ). ADC2. |
| 20   | ⚠️     | -                           | -                         | USB D+. ADC2. Conflicts with native USB if active.                   |
| 21   | ✅     | -                           | TFT_RST2 (left eye)       |                                                                      |
| 22   | N/A    | -                           | -                         | Pin does not exist on ESP32-S3                                       |
| 23   | N/A    | -                           | -                         | "                                                                    |
| 24   | N/A    | -                           | -                         | "                                                                    |
| 25   | N/A    | -                           | -                         | "                                                                    |
| 26   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 27   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 28   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 29   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 30   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 31   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 32   | ❌     | -                           | -                         | SPI flash / PSRAM                                                    |
| 33   | ❌     | -                           | -                         | SPI flash / PSRAM (Octal SPI variant)                                |
| 34   | ❌     | -                           | -                         | SPI flash / PSRAM (Octal SPI variant)                                |
| 35   | ❌     | -                           | -                         | Octal SPI PSRAM - always memory on this module                       |
| 36   | ❌     | -                           | -                         | Octal SPI PSRAM - always memory on this module                       |
| 37   | ❌     | -                           | -                         | Octal SPI PSRAM - always memory on this module                       |
| 38   | ✅     | TFT_DC                      | -                         |                                                                      |
| 39   | ✅     | TFT_MOSI                    | -                         |                                                                      |
| 40   | ✅     | TFT_SCLK                    | -                         |                                                                      |
| 41   | FREE   | -                           | -                         |                                                                      |
| 42   | ✅     | SERVO_8 (LEDC)              | SERVO_8 (LEDC)            |                                                                      |
| 43   | ⚠️     | -                           | -                         | UART0 TX (default Serial when CDC-on-boot is off)                    |
| 44   | ⚠️     | -                           | -                         | UART0 RX (default Serial when CDC-on-boot is off)                    |
| 45   | ⚠️     | -                           | -                         | Strapping (VDD_SPI). Avoid.                                          |
| 46   | ⚠️     | -                           | -                         | Strapping. Avoid.                                                    |
| 47   | FREE   | -                           | -                         |                                                                      |
| 48   | FREE   | -                           | -                         | On-board WS2812 RGB LED on most DevKitC-1 boards                     |

## Summary of free pins

### Unconditionally free

* **esp32s3 (face env):** GPIO 4, 5, 14, 20, 21, 41, 47, 48 (plus ADC2-caveat pins 9, 10, 11, 12, 13 which are free in this env but should not be used for ADC while WiFi is up)
* **esp32s3_eyes env:** GPIO 1, 2, 4, 5, 20, 38, 39, 40, 41, 47, 48

### Free with caveats

* GPIO 0, 3, 45, 46 - strapping pins; usable as outputs after boot but avoid for inputs that could be driven during reset
* GPIO 19 - free in both envs but is USB D− (avoid if native USB used)
* GPIO 20 - free in both envs but is USB D+ (avoid if native USB used)
* GPIO 43, 44 - free if you give up the UART0 Serial console
* GPIO 48 - usually wired to the board's on-board RGB LED

### ADC notes

* ADC1: GPIO 1–10 - works with WiFi
* ADC2: GPIO 11–20 - **cannot be used for ADC while WiFi is active**

## Current assignment recap

**Shared across both display envs:**

* Servos 1–8 on GPIO 6, 7, 15, 16, 17, 18, 8, 42 (LEDC channels 0–7)
* TFT SPI has no MISO, no backlight (the SCLK/MOSI/DC pins differ per env — see below)

**Face env ( `esp32s3` )** - single GC9A01 240×240:

* TFT SPI: SCLK=40, MOSI=39, DC=38
* CS=2, RST=1

**Eyes env ( `esp32s3_eyes` , `esp32s3_eyes_debug` )** - dual GC9D01 160×160:

* TFT SPI: SCLK=9, MOSI=10, DC=11
* Right eye (D0): CS1=12, RST1=13
* Left eye (D1): CS2=14, RST2=21
