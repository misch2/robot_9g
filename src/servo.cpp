#include "servo.h"

namespace {
constexpr uint32_t kServoFreqHz       = 50;
constexpr uint8_t kServoResolutionBit = 14;                         // 16-bit @ 50Hz fails ledcSetup on arduino-esp32 v2.x
constexpr uint32_t kServoPeriodUs     = 1000000u / kServoFreqHz;    // 20000 us
constexpr uint32_t kServoMaxDuty      = 1u << kServoResolutionBit;  // 16384

uint32_t pulseToDuty(int microseconds) {
    if (microseconds < 0) microseconds = 0;
    return ((uint64_t)microseconds * kServoMaxDuty) / kServoPeriodUs;
}
}  // namespace

Servo::Servo(uint8_t ledcChannel, int pinNumber, int minPulseLength, int maxPulseLength,
             int minAngle, int maxAngle, int neutralAngle)
    : ledcChannel(ledcChannel),
      pinNumber(pinNumber),
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
    pinMode(pinNumber, OUTPUT);
    double actualHz = ledcSetup(ledcChannel, kServoFreqHz, kServoResolutionBit);
    ledcAttachPin(pinNumber, ledcChannel);
    attached = (actualHz > 0.0);
    Serial.printf("Servo: attach pin=%d ch=%u req=%uHz/%ubit actual=%.2fHz %s\n",
                  pinNumber, (unsigned)ledcChannel,
                  (unsigned)kServoFreqHz, (unsigned)kServoResolutionBit,
                  actualHz,
                  (actualHz > 0.0) ? "OK" : "FAILED");
}

void Servo::detach() {
    if (!attached) return;
    ledcDetachPin(pinNumber);
    attached = false;
}

float Servo::clampAngle(float angle) const {
    if (angle < minAngle) return minAngle;
    if (angle > maxAngle) return maxAngle;
    return angle;
}

int Servo::angleToMicroseconds(float angle) const {
    // Linear map: 0..180 deg -> minPulseLength..maxPulseLength us. Done in
    // float for sub-degree precision lost when going through int angles.
    float us = minPulseLength + (angle / 180.0f) * (maxPulseLength - minPulseLength);
    return (int)lroundf(us);
}

void Servo::writeMicroseconds(int microseconds) {
    ledcWrite(ledcChannel, pulseToDuty(microseconds));
}

void Servo::setAngle(float angle) {
    attach();
    float clamped = clampAngle(angle);
    currentAngle  = clamped;
    int us        = angleToMicroseconds(clamped);
    if (us != lastPulseUs) {
        uint32_t duty = pulseToDuty(us);
        // Serial.printf("Servo: setAngle pin=%d ch=%u angle=%.1f -> %dus (duty=%u)\n",
        //               pinNumber, (unsigned)ledcChannel, clamped, us, (unsigned)duty);
        lastPulseUs = us;
    }
    writeMicroseconds(us);
}
