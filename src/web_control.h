#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

#include "servo_manager.h"
#include "servo_motion.h"

// Wi-Fi access point + HTTP/WebSocket interface for browser-based control.
// Routes /service to a per-servo "service menu" that drives ServoMotion
// directly (bypassing RobotMotion). The future main control UI will live at /.
class WebControl {
public:
    WebControl(ServoManager& manager, ServoMotion& motion);

    // Brings up the AP with credentials from wifi_config.h, mounts the HTTP
    // routes, and starts the server. Call once from setup() after Serial.
    void begin();

    // Cleans up dropped WebSocket clients. Call from loop().
    void update();

private:
    void handleWsMessage(AsyncWebSocketClient* client, const uint8_t* data, size_t len);
    void sendState(AsyncWebSocketClient* client);
    String buildStateJson() const;

    ServoManager& manager;
    ServoMotion& motion;
    AsyncWebServer server;
    AsyncWebSocket ws;
    DNSServer dnsServer;
};
