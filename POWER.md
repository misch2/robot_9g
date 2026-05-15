# Powering the robot

```mermaid
flowchart TB

subgraph inputs["Inputs"]
    direction TB
    main["Main input 5 - 24 V"]
    usbc["`USB‑C
_(alternative for debugging)_
`"]
end

subgraph Regulators
    direction TB
    stepdown3V3
    stepdown5
    stepdown6
end

subgraph lcds["LCDs"]
    direction LR
    eyes
    mouth
end

usbc -.-> esp32

main --> stepdown3V3[Step down 3.3 V]
main --> stepdown5[Step down 5 V]
main --> stepdown6[Step down 6 V]

stepdown3V3 -- "JST-XH" --> eyes["Eyes LCD"]
stepdown3V3 -- "JST-XH" --> mouth["Mouth LCD"]
stepdown3V3 -- "JST-XH" --> i2c["I2C bus"]

i2c --> servoboard["PCA9685
servo board"]
i2c --> ina219["INA219
current sensor"]

stepdown5 -- "JST-XH" --> audio["MAX98357A
audio amplifier"]
stepdown5 -- "JST-XH" --> esp32["ESP32S3
main CPU"]

stepdown6-- "Screw terminal" -->servos["MG90S servos"]
```

## ⚡ Main power input

Input range: 5 - 24 V (common range for all the step down converters)

In reality I'll be using either either 12 V from USB-C PD or 6.8 - 12.6 V from 2S or 3S Li-Ion etc.

### 3.3 V rail

Source: 3V3 Step down, https://www.aliexpress.com/item/1005008257960729.html
Input range: 5 - 30 V
Output current: 1.6 A (continuous)

Used by:

- eyes LCD
- mouth/face round LCD
- PCA9685 servo board chip
- I2C bus

### 5 V rail

Source: 5V adjustable step down, https://www.laskakit.cz/mikro-step-down-menic--nastavitelny/
Input range: 4.5 - 24 V
Output current: 3 A (max)

Used by:

- ESP32S3 (but maybe I can switch to 3V3 directly?)
- MAX98357A I2S amp

### 5.8 V rail

Source: adjustable step down, https://www.laskakit.cz/step-down-menic-s-xl4005/
Input range: 5V - 32 V
Output current: 2.5 A continuous, 5 A max (with cooling)

Used by:

- servos
