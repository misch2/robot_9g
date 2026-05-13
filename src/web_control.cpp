#include "web_control.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "wifi_config.h"

namespace {

// Landing page — placeholder for the future main control UI. Just a title and
// a link to the service menu so the menu is reachable without typing the path.
const char kLandingHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>robot_9g</title>
<style>
  :root { color-scheme: dark; }
  body { margin: 0; min-height: 100vh; display: flex; flex-direction: column;
         align-items: center; justify-content: center; gap: 1.5rem;
         font-family: system-ui, -apple-system, sans-serif;
         background: #111; color: #eee; padding: 2rem; }
  h1 { margin: 0; font-weight: 500; letter-spacing: .02em; }
  p  { margin: 0; color: #888; }
  a.btn { padding: .9rem 1.6rem; border-radius: .6rem; background: #2a6df4;
          color: #fff; text-decoration: none; font-weight: 500; }
  a.btn:active { background: #1f54c4; }
</style>
</head>
<body>
  <h1>robot_9g</h1>
  <p>Main control coming soon.</p>
  <a class="btn" href="/service">Service Menu</a>
</body>
</html>
)HTML";

// Service menu — multi-select servos, choose a target (neutral/min/max/custom
// angle), choose an easing, fire moves, see the easing curve.
const char kServiceHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>robot_9g — Service</title>
<style>
  :root { color-scheme: dark; --bg:#111; --panel:#1c1c1c; --line:#2a2a2a;
          --fg:#eee; --muted:#888; --accent:#2a6df4; --accent2:#f49a2a; }
  * { box-sizing: border-box; }
  body { margin: 0; font-family: system-ui, -apple-system, sans-serif;
         background: var(--bg); color: var(--fg); padding: 1rem;
         max-width: 720px; margin-inline: auto; }
  header { display: flex; align-items: baseline; justify-content: space-between;
           margin-bottom: 1rem; }
  header a { color: var(--muted); text-decoration: none; font-size: .9rem; }
  h1 { font-size: 1.2rem; font-weight: 500; margin: 0; }
  section { background: var(--panel); border-radius: .6rem; padding: 1rem;
            margin-bottom: 1rem; }
  section h2 { font-size: .8rem; font-weight: 600; margin: 0 0 .8rem;
               color: var(--muted); text-transform: uppercase; letter-spacing: .08em; }
  .row { display: flex; flex-wrap: wrap; gap: .5rem; }
  .chip { padding: .55rem .8rem; border-radius: .5rem; background: #262626;
          border: 1px solid var(--line); color: var(--fg); font: inherit;
          cursor: pointer; user-select: none; }
  .chip small { display: block; color: var(--muted); font-size: .7rem;
                margin-top: .15rem; }
  .chip[aria-pressed="true"] { background: var(--accent); border-color: var(--accent); }
  .chip[aria-pressed="true"] small { color: rgba(255,255,255,.85); }
  .bulk { margin-left: auto; display: flex; gap: .4rem; }
  .bulk button { background: transparent; border: 1px solid var(--line);
                 color: var(--muted); padding: .4rem .7rem; border-radius: .4rem;
                 font: inherit; cursor: pointer; }
  .actions button { padding: .7rem 1rem; border-radius: .5rem; border: none;
                    background: var(--accent); color: #fff; font: inherit;
                    cursor: pointer; }
  .actions button.secondary { background: #333; }
  .custom { display: flex; align-items: center; gap: .6rem; margin-top: .8rem; }
  .custom input[type=range] { flex: 1; }
  .custom output { min-width: 3.5em; text-align: right; color: var(--muted); }
  .easing-row label { display: inline-flex; align-items: center; gap: .35rem;
                      padding: .45rem .7rem; border-radius: .4rem;
                      border: 1px solid var(--line); cursor: pointer; }
  .easing-row input { accent-color: var(--accent); }
  .duration { display: flex; align-items: center; gap: .6rem; margin-top: .8rem; }
  .duration input { width: 6em; padding: .4rem .5rem; border-radius: .4rem;
                    border: 1px solid var(--line); background: #262626;
                    color: var(--fg); font: inherit; }
  svg.curve { width: 100%; height: 160px; display: block; }
  .legend { display: flex; gap: 1rem; font-size: .8rem; color: var(--muted);
            margin-top: .4rem; }
  .legend span::before { content: ""; display: inline-block; width: .8em;
                         height: .15em; vertical-align: middle; margin-right: .4em; }
  .legend .pos::before { background: var(--accent); }
  .legend .vel::before { background: var(--accent2); }
  #status { font-size: .85rem; color: var(--muted); min-height: 1.2em; }
  #status.error { color: #f55; }
  .power { display: flex; gap: 1.5rem; flex-wrap: wrap; font-variant-numeric: tabular-nums; }
  .power .item { display: flex; flex-direction: column; gap: .15rem; }
  .power .val { font-size: 1.4rem; font-weight: 500; }
  .power .unit { color: var(--muted); font-size: .75rem; text-transform: uppercase;
                 letter-spacing: .08em; }
</style>
</head>
<body>
  <header>
    <h1>Service Menu</h1>
    <a href="/">&larr; Home</a>
  </header>

  <section>
    <h2>Power</h2>
    <div class="power" id="power">
      <div class="item"><span class="val" id="pV">–</span><span class="unit">V</span></div>
      <div class="item"><span class="val" id="pA">–</span><span class="unit">mA</span></div>
      <div class="item"><span class="val" id="pW">–</span><span class="unit">mW</span></div>
    </div>
  </section>

  <section>
    <h2>Servos</h2>
    <div class="row" id="servos"></div>
    <div class="row bulk">
      <button id="all">All</button>
      <button id="none">None</button>
    </div>
  </section>

  <section>
    <h2>Target</h2>
    <div class="row actions">
      <button data-target="neutral">Neutral (rest)</button>
      <button data-target="min">Min (-1)</button>
      <button data-target="max">Max (+1)</button>
    </div>
    <div class="custom">
      <input type="range" id="angle" min="0" max="180" step="0.5" value="90">
      <output id="angleOut">90.0&deg;</output>
      <button class="secondary" id="applyAngle">Apply angle</button>
    </div>
  </section>

  <section>
    <h2>Easing</h2>
    <div class="row easing-row" id="easing">
      <label><input type="radio" name="easing" value="Linear">Linear</label>
      <label><input type="radio" name="easing" value="EaseIn">EaseIn</label>
      <label><input type="radio" name="easing" value="EaseOut">EaseOut</label>
      <label><input type="radio" name="easing" value="EaseInOut" checked>EaseInOut</label>
    </div>
    <div class="duration">
      <span style="color:var(--muted)">Duration</span>
      <input type="number" id="duration" min="0" max="10000" step="50" value="600">
      <span style="color:var(--muted)">ms</span>
    </div>
    <svg class="curve" viewBox="0 0 200 100" preserveAspectRatio="none" id="curve">
      <line x1="0" y1="100" x2="200" y2="100" stroke="#333" stroke-width="0.5"/>
      <line x1="0" y1="0"   x2="0"   y2="100" stroke="#333" stroke-width="0.5"/>
      <polyline id="pos" fill="none" stroke="#2a6df4" stroke-width="1.5"/>
      <polyline id="vel" fill="none" stroke="#f49a2a" stroke-width="1" stroke-dasharray="2 2"/>
    </svg>
    <div class="legend"><span class="pos">position</span><span class="vel">velocity</span></div>
  </section>

  <div id="status"></div>

<script>
const easings = {
  Linear:    t => t,
  EaseIn:    t => t*t,
  EaseOut:   t => 1 - (1-t)*(1-t),
  EaseInOut: t => t < 0.5 ? 2*t*t : 1 - 2*(1-t)*(1-t),
};

let state = { servos: [] };
const selected = new Set();
let ws;

const $ = id => document.getElementById(id);
const status = (msg, isError=false) => {
  $('status').textContent = msg;
  $('status').classList.toggle('error', isError);
};

function renderServos() {
  const root = $('servos');
  root.innerHTML = '';
  for (const s of state.servos) {
    const btn = document.createElement('button');
    btn.className = 'chip';
    btn.dataset.id = s.id;
    btn.setAttribute('aria-pressed', selected.has(s.id) ? 'true' : 'false');
    btn.innerHTML = `${s.name}<small>${s.angle.toFixed(1)}&deg; (rest ${s.rest.toFixed(0)})</small>`;
    btn.onclick = () => {
      if (selected.has(s.id)) selected.delete(s.id);
      else selected.add(s.id);
      btn.setAttribute('aria-pressed', selected.has(s.id) ? 'true' : 'false');
    };
    root.appendChild(btn);
  }
}

function renderPower() {
  const p = state.power;
  if (!p || p.ready === false) {
    $('pV').textContent = $('pA').textContent = $('pW').textContent = '–';
    return;
  }
  $('pV').textContent = p.v.toFixed(2);
  $('pA').textContent = p.ma.toFixed(0);
  $('pW').textContent = p.mw.toFixed(0);
}

function selectedIds() { return [...selected]; }

function send(msg) {
  if (!ws || ws.readyState !== 1) { status('WebSocket not connected', true); return; }
  ws.send(JSON.stringify(msg));
}

function move(target) {
  const ids = selectedIds();
  if (ids.length === 0) { status('Select at least one servo', true); return; }
  const easing = document.querySelector('input[name=easing]:checked').value;
  const durationMs = Number($('duration').value) || 0;
  send({ cmd: 'move', ids, target, easing, durationMs });
  status(`Sent: ${ids.length} servo(s) → ${target}, ${easing}, ${durationMs}ms`);
}

function drawCurve() {
  const easing = document.querySelector('input[name=easing]:checked').value;
  const fn = easings[easing];
  const N = 60;
  const pos = [], vel = [];
  let prev = 0;
  for (let i = 0; i <= N; i++) {
    const t = i / N;
    const y = fn(t);
    pos.push(`${(t*200).toFixed(2)},${(100 - y*100).toFixed(2)}`);
    if (i > 0) {
      const v = (y - prev) * N;        // dy/dt sampled, scale ~1 at peak for Linear
      vel.push(`${(t*200).toFixed(2)},${(100 - v*50).toFixed(2)}`);
    }
    prev = y;
  }
  $('pos').setAttribute('points', pos.join(' '));
  $('vel').setAttribute('points', vel.join(' '));
}

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen    = () => status('Connected');
  ws.onclose   = () => { status('Disconnected — reconnecting…', true); setTimeout(connect, 1500); };
  ws.onerror   = () => status('WebSocket error', true);
  ws.onmessage = ev => {
    try {
      const m = JSON.parse(ev.data);
      if (m.type === 'state') { state = m; renderServos(); renderPower(); }
    } catch (e) { console.error(e); }
  };
}

document.querySelectorAll('.actions button[data-target]').forEach(b => {
  b.onclick = () => move(b.dataset.target);
});
$('all').onclick  = () => { state.servos.forEach(s => selected.add(s.id)); renderServos(); };
$('none').onclick = () => { selected.clear(); renderServos(); };
$('angle').oninput = e => $('angleOut').textContent = `${Number(e.target.value).toFixed(1)}°`;
$('applyAngle').onclick = () => move(Number($('angle').value));
document.querySelectorAll('input[name=easing]').forEach(r => r.onchange = drawCurve);

drawCurve();
connect();
</script>
</body>
</html>
)HTML";

// Decode a "target" field from an incoming move command into a fraction-or-angle
// instruction. Returns true on success.
struct MoveTarget {
    enum Kind { Neutral, Fraction, Angle } kind;
    float value;  // fraction in [-1,+1] for Fraction; degrees for Angle; unused for Neutral
};

bool parseTarget(JsonVariantConst v, MoveTarget& out) {
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (!strcmp(s, "neutral")) { out.kind = MoveTarget::Neutral;  out.value = 0; return true; }
        if (!strcmp(s, "min"))     { out.kind = MoveTarget::Fraction; out.value = -1.0f; return true; }
        if (!strcmp(s, "max"))     { out.kind = MoveTarget::Fraction; out.value = +1.0f; return true; }
        return false;
    }
    if (v.is<float>() || v.is<int>()) {
        out.kind = MoveTarget::Angle;
        out.value = v.as<float>();
        return true;
    }
    return false;
}

Easing parseEasing(const char* s) {
    if (s) {
        if (!strcmp(s, "Linear"))    return Easing::Linear;
        if (!strcmp(s, "EaseIn"))    return Easing::EaseIn;
        if (!strcmp(s, "EaseOut"))   return Easing::EaseOut;
    }
    return Easing::EaseInOut;  // default
}

}  // namespace

WebControl::WebControl(ServoManager& manager, ServoMotion& motion, const CurrentSensor& current)
    : manager(manager), motion(motion), current(current), server(80), ws("/ws") {}

void WebControl::begin() {
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(kApSsid, kApPassword);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("WebControl: AP %s (%s), IP=%s\n", kApSsid, ok ? "up" : "FAILED",
                  ip.toString().c_str());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html; charset=utf-8", kLandingHtml);
    });
    server.on("/service", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html; charset=utf-8", kServiceHtml);
    });
    // Captive portal: any other URL (including OS probe URLs like
    // /generate_204, /hotspot-detect.html, /ncsi.txt) returns the landing
    // page. Combined with the wildcard DNS server below this triggers the
    // "sign in to Wi-Fi" popup on iOS/Android/Windows.
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(200, "text/html; charset=utf-8", kLandingHtml);
    });

    // Resolve every DNS query to our AP IP so the OS captive-portal probe
    // hits this server instead of the real internet.
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", ip);

    ws.onEvent([this](AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type,
                      void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("WebControl: WS client #%u connected\n", client->id());
                sendState(client);
                break;
            case WS_EVT_DISCONNECT:
                Serial.printf("WebControl: WS client #%u disconnected\n", client->id());
                break;
            case WS_EVT_DATA: {
                AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
                if (info->final && info->index == 0 && info->len == len &&
                    info->opcode == WS_TEXT) {
                    handleWsMessage(client, data, len);
                }
                break;
            }
            default:
                break;
        }
    });
    server.addHandler(&ws);
    server.begin();
    Serial.println("WebControl: HTTP server listening on :80");
}

void WebControl::update() {
    dnsServer.processNextRequest();
    ws.cleanupClients();
    uint32_t now = millis();
    if (now - lastBroadcastMs >= 1000) {
        lastBroadcastMs = now;
        if (ws.count() > 0) {
            ws.textAll(buildStateJson());
        }
    }
}

void WebControl::handleWsMessage(AsyncWebSocketClient* client, const uint8_t* data, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
        Serial.printf("WebControl: JSON parse error: %s\n", err.c_str());
        return;
    }
    const char* cmd = doc["cmd"] | "";
    if (strcmp(cmd, "move") != 0) {
        Serial.printf("WebControl: unknown cmd '%s'\n", cmd);
        return;
    }
    MoveTarget target;
    if (!parseTarget(doc["target"], target)) {
        Serial.println("WebControl: invalid target");
        return;
    }
    Easing easing = parseEasing(doc["easing"] | "EaseInOut");
    uint32_t durationMs = doc["durationMs"] | 0u;

    JsonArrayConst ids = doc["ids"].as<JsonArrayConst>();
    if (ids.isNull()) {
        Serial.println("WebControl: ids missing");
        return;
    }
    for (JsonVariantConst v : ids) {
        int raw = v.as<int>();
        if (raw < 0 || raw >= static_cast<int>(ServoId::_Count)) continue;
        ServoId id = static_cast<ServoId>(raw);
        switch (target.kind) {
            case MoveTarget::Neutral:
                motion.moveToFraction(id, 0.0f, durationMs, easing);
                break;
            case MoveTarget::Fraction:
                motion.moveToFraction(id, target.value, durationMs, easing);
                break;
            case MoveTarget::Angle:
                motion.moveTo(id, target.value, durationMs, easing);
                break;
        }
    }
    sendState(client);
}

void WebControl::sendState(AsyncWebSocketClient* client) {
    String json = buildStateJson();
    client->text(json);
}

String WebControl::buildStateJson() const {
    JsonDocument doc;
    doc["type"] = "state";
    JsonArray arr = doc["servos"].to<JsonArray>();
    for (size_t i = 0; i < ServoManager::kCount; i++) {
        const ServoSpec& spec = kServos[i];
        ServoId id = static_cast<ServoId>(i);
        JsonObject o = arr.add<JsonObject>();
        o["id"]   = static_cast<int>(i);
        o["name"] = spec.name;
        o["angle"] = manager.getCurrentAngle(id);
        o["min"]   = spec.minAngle;
        o["max"]   = spec.maxAngle;
        o["rest"]  = spec.restAngle;
    }
    JsonObject p = doc["power"].to<JsonObject>();
    if (current.isReady()) {
        p["v"]  = current.getBusVoltageV();
        p["ma"] = current.getCurrentMa();
        p["mw"] = current.getPowerMw();
    } else {
        p["ready"] = false;
    }
    String out;
    serializeJson(doc, out);
    return out;
}
