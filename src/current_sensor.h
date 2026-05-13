#pragma once

#include <Arduino.h>

class Adafruit_INA219;

// Thin wrapper around an INA219 high-side current sensor on the shared I²C
// bus. begin() is idempotent on Wire — safe to call after another driver
// (e.g. the PCA9685 servo backend) has already brought the bus up.
class CurrentSensor {
public:
    explicit CurrentSensor(uint8_t i2cAddress = 0x41);
    ~CurrentSensor();

    // Brings up Wire on the given pins if not already started, then probes
    // the INA219. Returns true if the sensor acknowledged.
    bool begin(int sdaPin, int sclPin);

    // Pulls fresh values from the chip (four I²C reads). Cheap enough to
    // call at a few Hz from loop().
    void read();

    float getBusVoltageV() const { return busVoltageV; }
    float getShuntVoltageMv() const { return shuntVoltageMv; }
    float getCurrentMa() const { return currentMa; }
    float getPowerMw() const { return powerMw; }
    bool isReady() const { return started; }

private:
    uint8_t address;
    bool started;
    Adafruit_INA219* driver;
    float busVoltageV;
    float shuntVoltageMv;
    float currentMa;
    float powerMw;
};
