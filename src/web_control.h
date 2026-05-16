#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

#include "current_sensor.h"
#include "robot_eyes.h"
#include "robot_face.h"
#include "robot_motion.h"
#include "servo_manager.h"
#include "servo_motion.h"

// Wi-Fi access point + HTTP/WebSocket interface for browser-based control.
// Landing page at / is a menu that links to /control (high-level: queued
// movements, poses, face expressions — drives RobotMotion + RobotFace/Eyes)
// and /service (low-level: per-servo target/easing testing — drives
// ServoMotion directly, bypassing RobotMotion).
class WebControl {
public:
    WebControl(ServoManager& manager, ServoMotion& motion, RobotMotion& robot,
               RobotFace& face, RobotEyes& eyes, const CurrentSensor& current);

    // Brings up the AP with credentials from wifi_config.h, mounts the HTTP
    // routes, and starts the server. Call once from setup() after Serial.
    void begin();

    // Cleans up dropped WebSocket clients and periodically broadcasts state
    // (including live power readings) to all connected clients. Call from loop().
    void update();

private:
    void handleWsMessage(AsyncWebSocketClient* client, const uint8_t* data, size_t len);
    void sendState(AsyncWebSocketClient* client);
    String buildStateJson() const;

    ServoManager& manager;
    ServoMotion& motion;
    RobotMotion& robot;
    RobotFace& face;
    RobotEyes& eyes;
    const CurrentSensor& current;
    AsyncWebServer server;
    AsyncWebSocket ws;
    DNSServer dnsServer;
    uint32_t lastBroadcastMs = 0;
};
