#include "current_sensor.h"

#if defined(INA219_ADDR)

#include <Adafruit_INA219.h>
#include <Wire.h>

CurrentSensor::CurrentSensor(uint8_t i2cAddress)
    : address(i2cAddress),
      started(false),
      driver(nullptr),
      busVoltageV(0.0f),
      shuntVoltageMv(0.0f),
      currentMa(0.0f),
      powerMw(0.0f) {}

CurrentSensor::~CurrentSensor() {
    delete driver;
}

bool CurrentSensor::begin(int sdaPin, int sclPin) {
    if (started) return true;
    // Wire.begin() is idempotent on arduino-esp32 — if the PCA9685 backend
    // already brought the bus up on these pins, this is a no-op.
    Wire.begin(sdaPin, sclPin);
    driver  = new Adafruit_INA219(address);
    started = driver->begin(&Wire);
    if (!started) {
        Serial.printf("CurrentSensor[INA219]: NOT FOUND at 0x%02X (SDA=%d SCL=%d)\n",
                      (unsigned)address, sdaPin, sclPin);
        return false;
    }
    // Default calibration: 32 V bus, 2 A max, 100 µA resolution. Switch to
    // setCalibration_16V_400mA() if the rig draws <400 mA for more headroom.
    driver->setCalibration_32V_2A();
    Serial.printf("CurrentSensor[INA219]: up at 0x%02X (SDA=%d SCL=%d)\n",
                  (unsigned)address, sdaPin, sclPin);
    return true;
}

void CurrentSensor::read() {
    if (!started || !driver) return;
    shuntVoltageMv = driver->getShuntVoltage_mV();
    busVoltageV    = driver->getBusVoltage_V();
    currentMa      = driver->getCurrent_mA();
    powerMw        = driver->getPower_mW();
}

#else  // !INA219_ADDR — stubs so the LEDC env still links without the lib

CurrentSensor::CurrentSensor(uint8_t i2cAddress)
    : address(i2cAddress),
      started(false),
      driver(nullptr),
      busVoltageV(0.0f),
      shuntVoltageMv(0.0f),
      currentMa(0.0f),
      powerMw(0.0f) {}

CurrentSensor::~CurrentSensor() {}

bool CurrentSensor::begin(int, int) { return false; }
void CurrentSensor::read() {}

#endif
