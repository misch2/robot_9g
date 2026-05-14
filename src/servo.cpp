#include "servo.h"

#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

#ifndef PIN_I2C_SDA
#error "PIN_I2C_SDA must be defined (-D in platformio.ini)"
#endif
#ifndef PIN_I2C_SCL
#error "PIN_I2C_SCL must be defined (-D in platformio.ini)"
#endif
#ifndef PCA9685_ADDR
#define PCA9685_ADDR 0x40
#endif

namespace {
constexpr uint32_t kServoFreqHz = 50;
// 400 kHz with external 4.7 kΩ pull-ups on SDA/SCL.
constexpr uint32_t kI2cClockHz = 400000;
constexpr uint16_t kPwmOff     = 4096;  // "fully off" sentinel in the PCA9685 OFF count

// Shared driver. Held as a pointer rather than a static object so the I2C
// transactions in its ctor are deferred until servoBackendBegin() runs after
// Serial/Wire are up.
Adafruit_PWMServoDriver* g_driver = nullptr;
bool g_started                    = false;
}  // namespace

bool servoBackendBegin() {
    if (g_started) return true;
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(kI2cClockHz);
    // Probe the chip before letting Adafruit_PWMServoDriver::begin() blindly
    // poke registers — that call returns void, so without this we'd silently
    // run with a non-existent servo bus.
    Wire.beginTransmission(PCA9685_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.printf("Servo[PCA9685]: NOT FOUND at 0x%02X (SDA=%d SCL=%d)\n",
                      (unsigned)PCA9685_ADDR, (int)PIN_I2C_SDA, (int)PIN_I2C_SCL);
        return false;
    }
    g_driver = new Adafruit_PWMServoDriver(PCA9685_ADDR, Wire);
    g_driver->begin();
    g_driver->setOscillatorFrequency(27000000);  // Adafruit-recommended default; calibrate if measured pulses are off
    g_driver->setPWMFreq(kServoFreqHz);
    g_started = true;
    Serial.printf("Servo[PCA9685]: backend up addr=0x%02X SDA=%d SCL=%d freq=%uHz\n",
                  (unsigned)PCA9685_ADDR, (int)PIN_I2C_SDA, (int)PIN_I2C_SCL,
                  (unsigned)kServoFreqHz);
    return true;
}

Servo::Servo(uint8_t channel, int minPulseLength, int maxPulseLength,
             int minAngle, int maxAngle, int neutralAngle)
    : channel(channel),
      minPulseLength(minPulseLength),
      maxPulseLength(maxPulseLength),
      minAngle(minAngle),
      maxAngle(maxAngle),
      neutralAngle(neutralAngle),
      currentAngle((float)neutralAngle),
      attached(false),
      lastPulseUs(-1) {}

void Servo::attach() {
    if (attached) return;
    if (!g_driver) {
        Serial.printf("Servo[PCA9685]: attach ch=%u FAILED — call servoBackendBegin() first\n",
                      (unsigned)channel);
        return;
    }
    attached = true;
    Serial.printf("Servo[PCA9685]: attach ch=%u\n", (unsigned)channel);
}

void Servo::detach() {
    if (!attached) return;
    if (g_driver) g_driver->setPWM(channel, 0, kPwmOff);
    attached = false;
}

float Servo::clampAngle(float angle) const {
    if (angle < minAngle) return minAngle;
    if (angle > maxAngle) return maxAngle;
    return angle;
}

int Servo::angleToMicroseconds(float angle) const {
    float us = minPulseLength + (angle / 180.0f) * (maxPulseLength - minPulseLength);
    return (int)lroundf(us);
}

void Servo::writeMicroseconds(int microseconds) {
    if (!g_driver) return;
    g_driver->writeMicroseconds(channel, (uint16_t)microseconds);
}

void Servo::setAngle(float angle) {
    attach();
    float clamped = clampAngle(angle);
    currentAngle  = clamped;
    int us        = angleToMicroseconds(clamped);
    if (us != lastPulseUs) lastPulseUs = us;
    writeMicroseconds(us);
}
