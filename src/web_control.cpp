#include "web_control.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "wifi_config.h"

namespace {

// Landing page — main menu. Links to the regular control UI (/control) and
// the low-level per-servo service menu (/service).
const char kLandingHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>robot_9g</title>
<style>
  :root { color-scheme: dark; }
  body { margin: 0; min-height: 100vh; display: flex; flex-direction: column;
         align-items: center; justify-content: center; gap: 1.2rem;
         font-family: system-ui, -apple-system, sans-serif;
         background: #111; color: #eee; padding: 2rem; }
  h1 { margin: 0 0 .5rem; font-weight: 500; letter-spacing: .02em; }
  .menu { display: flex; flex-direction: column; gap: .8rem; width: min(20rem, 100%); }
  a.btn { display: block; padding: 1rem 1.2rem; border-radius: .6rem;
          text-decoration: none; font-weight: 500; text-align: center; }
  a.btn small { display: block; font-weight: 400; opacity: .75;
                font-size: .8rem; margin-top: .25rem; }
  a.primary   { background: #2a6df4; color: #fff; }
  a.primary:active   { background: #1f54c4; }
  a.secondary { background: #2a2a2a; color: #eee; border: 1px solid #333; }
  a.secondary:active { background: #1f1f1f; }
</style>
</head>
<body>
  <h1>robot_9g</h1>
  <nav class="menu">
    <a class="btn primary"   href="/control">Control<small>movement, poses, expressions</small></a>
    <a class="btn secondary" href="/service">Service Menu<small>per-servo testing</small></a>
  </nav>
</body>
</html>
)HTML";

// Main control UI — high-level commands routed through RobotMotion and the
// face/eye displays. Communicates over the shared /ws WebSocket.
const char kControlHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>robot_9g — Control</title>
<style>
  :root { color-scheme: dark; --bg:#111; --panel:#1c1c1c; --line:#2a2a2a;
          --fg:#eee; --muted:#888; --accent:#2a6df4; --accent2:#f49a2a; }
  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
  body { margin: 0; font-family: system-ui, -apple-system, sans-serif;
         background: var(--bg); color: var(--fg); padding: 1rem;
         max-width: 480px; margin-inline: auto; }
  header { display: flex; align-items: baseline; justify-content: space-between;
           margin-bottom: 1rem; }
  header a { color: var(--muted); text-decoration: none; font-size: .9rem; }
  h1 { font-size: 1.2rem; font-weight: 500; margin: 0; }
  section { background: var(--panel); border-radius: .6rem; padding: 1rem;
            margin-bottom: 1rem; }
  section h2 { font-size: .8rem; font-weight: 600; margin: 0 0 .8rem;
               color: var(--muted); text-transform: uppercase; letter-spacing: .08em; }
  /* Movement D-pad */
  .pad { display: grid; grid-template-columns: 1fr 1fr 1fr;
         grid-template-rows: 1fr 1fr 1fr; gap: .5rem;
         aspect-ratio: 3 / 2.6; max-width: 360px; margin: 0 auto; }
  .pad button { border: none; border-radius: .6rem; background: #262626;
                color: var(--fg); font: inherit; font-size: 1.2rem;
                cursor: pointer; touch-action: manipulation;
                display: flex; align-items: center; justify-content: center; }
  .pad button:active { background: var(--accent); }
  .pad .fwd  { grid-column: 2; grid-row: 1; }
  .pad .left { grid-column: 1; grid-row: 2; }
  .pad .stop { grid-column: 2; grid-row: 2; background: #3a1f1f; font-size: .9rem; }
  .pad .stop:active { background: #6a2a2a; }
  .pad .right{ grid-column: 3; grid-row: 2; }
  .pad .back { grid-column: 2; grid-row: 3; }
  /* Generic chip rows */
  .row { display: flex; flex-wrap: wrap; gap: .5rem; }
  .chip { padding: .7rem 1rem; border-radius: .5rem; background: #262626;
          border: 1px solid var(--line); color: var(--fg); font: inherit;
          cursor: pointer; touch-action: manipulation; flex: 1 1 auto;
          min-width: 5.5rem; text-align: center; }
  .chip:active { background: #1f1f1f; }
  .chip[aria-pressed="true"] { background: var(--accent); border-color: var(--accent); }
  /* Legs grid: leg name + Up + Down */
  .legs { display: grid; grid-template-columns: auto 1fr 1fr; gap: .4rem .5rem;
          align-items: center; }
  .legs .leg-name { color: var(--muted); font-size: .9rem; padding-left: .2rem; }
  /* Head pad */
  .padwrap { display: flex; flex-direction: column; align-items: center; gap: .8rem; }
  #headPad { touch-action: none; user-select: none; max-width: 100%; }
  #headPad circle.bg { fill: #1c1c1c; stroke: #444; stroke-width: 1; }
  #headPad line.cross { stroke: #333; stroke-width: 0.5; }
  #headPad circle.dot { fill: var(--accent); transition: cx .08s, cy .08s; }
  /* Status strip */
  .strip { display: flex; align-items: center; justify-content: space-between;
           gap: .8rem; font-size: .85rem; }
  .pill { display: inline-flex; align-items: center; gap: .4rem;
          padding: .25rem .6rem; border-radius: 999px; background: #262626;
          border: 1px solid var(--line); color: var(--muted); }
  .pill::before { content: ""; display: inline-block; width: .55rem; height: .55rem;
                  border-radius: 50%; background: #555; }
  .pill.idle::before { background: #4caf50; }
  .pill.busy::before { background: var(--accent2); }
  .power { font-variant-numeric: tabular-nums; color: var(--muted); }
  #status { font-size: .85rem; color: var(--muted); min-height: 1.2em;
            margin-top: .3rem; }
  #status.error { color: #f55; }
</style>
</head>
<body>
  <header>
    <h1>Control</h1>
    <a href="/">&larr; Menu</a>
  </header>

  <section>
    <div class="strip">
      <span class="pill" id="motionPill">…</span>
      <span class="power" id="power">– V  – mA</span>
    </div>
    <div id="status"></div>
  </section>

  <section>
    <h2>Move</h2>
    <div class="pad">
      <button class="fwd"   data-act="step"   data-arg="1"  aria-label="Forward">&#9650;</button>
      <button class="left"  data-act="rotate" data-arg="-1"  aria-label="Rotate left">&#8634;</button>
      <button class="stop"  data-act="stand"               aria-label="Stand / stop">STAND</button>
      <button class="right" data-act="rotate" data-arg="1" aria-label="Rotate right">&#8635;</button>
      <button class="back"  data-act="step"   data-arg="-1" aria-label="Backward">&#9660;</button>
    </div>
  </section>

  <section>
    <h2>Pose</h2>
    <div class="row">
      <button class="chip" data-act="stand">Stand</button>
      <button class="chip" data-act="crouch">Crouch</button>
      <button class="chip" data-act="sit">Sit</button>
    </div>
  </section>

  <section>
    <h2>Tricks</h2>
    <div class="row">
      <button class="chip" data-act="dance">Dance</button>
      <button class="chip" data-act="shake">Shake No</button>
    </div>
  </section>

  <section>
    <h2>Legs</h2>
    <div class="legs">
      <span class="leg-name">Front Left</span>
      <button class="chip" data-act="legUp"   data-arg="0">Up</button>
      <button class="chip" data-act="legDown" data-arg="0">Down</button>
      <span class="leg-name">Front Right</span>
      <button class="chip" data-act="legUp"   data-arg="1">Up</button>
      <button class="chip" data-act="legDown" data-arg="1">Down</button>
      <span class="leg-name">Rear Left</span>
      <button class="chip" data-act="legUp"   data-arg="2">Up</button>
      <button class="chip" data-act="legDown" data-arg="2">Down</button>
      <span class="leg-name">Rear Right</span>
      <button class="chip" data-act="legUp"   data-arg="3">Up</button>
      <button class="chip" data-act="legDown" data-arg="3">Down</button>
    </div>
  </section>

  <section>
    <h2>Head &amp; Body</h2>
    <div class="padwrap">
      <svg id="headPad" viewBox="-100 -100 200 200" width="220" height="220"
           aria-label="Drag to aim head; tap centre to reset.">
        <circle class="bg" cx="0" cy="0" r="95"/>
        <line class="cross" x1="-95" y1="0" x2="95" y2="0"/>
        <line class="cross" x1="0" y1="-95" x2="0" y2="95"/>
        <circle class="dot" id="headDot" cx="0" cy="0" r="10"/>
      </svg>
      <div class="row">
        <button class="chip" id="headCenter">Centre head</button>
        <button class="chip" data-act="bodyNeutral">Neutral body</button>
      </div>
    </div>
  </section>

  <section>
    <h2>Expression</h2>
    <div class="row" id="expressions"></div>
  </section>

  <section>
    <h2>Eyes</h2>
    <div class="row">
      <button class="chip" data-act="eyeCycle">Cycle test image</button>
      <button class="chip" data-act="eyeIdentify">Identify L/R</button>
    </div>
  </section>

<script>
const EXPRESSIONS = ["Happy","Neutral","Curious","Concentrating","Worried","Sad","Surprised"];
let state = {};
let ws;

const $ = id => document.getElementById(id);
const status = (msg, isError=false) => {
  $('status').textContent = msg;
  $('status').classList.toggle('error', isError);
};

function send(msg) {
  if (!ws || ws.readyState !== 1) { status('WebSocket not connected', true); return; }
  ws.send(JSON.stringify(msg));
}

function renderExpressions() {
  const root = $('expressions');
  if (root.children.length === 0) {
    for (const name of EXPRESSIONS) {
      const btn = document.createElement('button');
      btn.className = 'chip';
      btn.textContent = name;
      btn.dataset.expr = name;
      btn.onclick = () => send({ cmd: 'expression', name });
      root.appendChild(btn);
    }
  }
  for (const btn of root.children) {
    btn.setAttribute('aria-pressed', btn.dataset.expr === state.expression ? 'true' : 'false');
  }
}

function renderStrip() {
  const pill = $('motionPill');
  if (state.idle === true)       { pill.className = 'pill idle'; pill.textContent = 'Idle'; }
  else if (state.idle === false) { pill.className = 'pill busy'; pill.textContent = 'Moving'; }
  else                           { pill.className = 'pill';      pill.textContent = '…'; }
  const p = state.power;
  if (p && p.ready !== false) {
    $('power').textContent = `${p.v.toFixed(2)} V  ${p.ma.toFixed(0)} mA`;
  } else {
    $('power').textContent = '– V  – mA';
  }
}

document.querySelectorAll('button[data-act]').forEach(b => {
  b.onclick = () => {
    const name = b.dataset.act;
    const arg  = b.dataset.arg !== undefined ? Number(b.dataset.arg) : 0;
    send({ cmd: 'action', name, arg });
    status(`Sent: ${name}${b.dataset.arg !== undefined ? ' ' + arg : ''}`);
  };
});

// Head pad: drag inside the circle to set HeadPan/HeadTilt.
// Sign convention: dragging right → robot looks right (HeadPan -1 in firmware
// terms, since +1 there means "turn left"); dragging up → tilt up.
(function() {
  const pad = $('headPad');
  const dot = $('headDot');
  const R   = 95;            // viewBox circle radius
  let dragging = false, lastSendMs = 0;

  function sendHead(pan, tilt) {
    const now = performance.now();
    if (now - lastSendMs < 60) return;
    lastSendMs = now;
    send({ cmd: 'head', pan, tilt });
  }

  function setFromEvent(ev) {
    const rect = pad.getBoundingClientRect();
    const scale = 200 / rect.width;  // viewBox units per CSS pixel
    let x = (ev.clientX - rect.left - rect.width/2)  * scale;
    let y = (ev.clientY - rect.top  - rect.height/2) * scale;
    const d = Math.hypot(x, y);
    if (d > R) { x *= R/d; y *= R/d; }
    dot.setAttribute('cx', x);
    dot.setAttribute('cy', y);
    sendHead(-x / R, -y / R);
  }

  pad.addEventListener('pointerdown', e => {
    dragging = true;
    pad.setPointerCapture(e.pointerId);
    setFromEvent(e);
  });
  pad.addEventListener('pointermove', e => { if (dragging) setFromEvent(e); });
  pad.addEventListener('pointerup',   e => {
    dragging = false;
    setFromEvent(e);
  });

  $('headCenter').onclick = () => {
    dot.setAttribute('cx', 0);
    dot.setAttribute('cy', 0);
    lastSendMs = 0;
    sendHead(0, 0);
    status('Sent: head 0,0');
  };
})();

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen    = () => status('Connected');
  ws.onclose   = () => { status('Disconnected — reconnecting…', true); setTimeout(connect, 1500); };
  ws.onerror   = () => status('WebSocket error', true);
  ws.onmessage = ev => {
    try {
      const m = JSON.parse(ev.data);
      if (m.type === 'state') { state = m; renderStrip(); renderExpressions(); }
    } catch (e) { console.error(e); }
  };
}

renderExpressions();
connect();
</script>
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

// Duration used by direct-servo commands fired from the /control page (legs,
// head pad, body neutral). Mirrors kDirectMoveMs in main.cpp; both get scaled
// by RobotConfig::speedFactor at call site so they stay in sync with the
// rest of the robot when it's slowed for debugging.
constexpr uint32_t kWebDirectMs = 250;

Easing parseEasing(const char* s) {
    if (s) {
        if (!strcmp(s, "Linear"))    return Easing::Linear;
        if (!strcmp(s, "EaseIn"))    return Easing::EaseIn;
        if (!strcmp(s, "EaseOut"))   return Easing::EaseOut;
    }
    return Easing::EaseInOut;  // default
}

}  // namespace

WebControl::WebControl(ServoManager& manager, ServoMotion& motion, RobotMotion& robot,
                       RobotFace& face, RobotEyes& eyes, const CurrentSensor& current)
    : manager(manager), motion(motion), robot(robot), face(face), eyes(eyes),
      current(current), server(80), ws("/ws") {}

void WebControl::begin() {
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(kApSsid, kApPassword);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("WebControl: AP %s (%s), IP=%s\n", kApSsid, ok ? "up" : "FAILED",
                  ip.toString().c_str());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html; charset=utf-8", kLandingHtml);
    });
    server.on("/control", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html; charset=utf-8", kControlHtml);
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
    if (!strcmp(cmd, "action")) {
        const char* name = doc["name"] | "";
        int arg          = doc["arg"] | 0;
        if (!strcmp(name, "step")) {
            robot.step(arg != 0 ? arg : 1);
        } else if (!strcmp(name, "rotate")) {
            int sign = arg >= 0 ? 1 : -1;
            robot.rotate(sign * robot.config.degreesPerRotation);
        } else if (!strcmp(name, "stand")) {
            robot.stand();
        } else if (!strcmp(name, "crouch")) {
            robot.crouch();
        } else if (!strcmp(name, "sit")) {
            robot.sit();
        } else if (!strcmp(name, "dance")) {
            robot.dance();
        } else if (!strcmp(name, "shake")) {
            robot.shakeNo();
        } else if (!strcmp(name, "legUp") || !strcmp(name, "legDown")) {
            // arg 0..3 maps directly to ServoId::FrontLeft..RearRight (the
            // first four entries of the enum, see config.h).
            if (arg >= 0 && arg <= 3) {
                ServoId leg = static_cast<ServoId>(arg);
                float frac = !strcmp(name, "legUp") ? robot.config.liftFraction : 0.0f;
                motion.moveToFraction(leg, frac, robot.config.scaled(kWebDirectMs));
            }
        } else if (!strcmp(name, "bodyNeutral")) {
            // Recentre Translation/Rotation/Head — leaves legs alone. Same as
            // the 'b' debug key.
            uint32_t ms = robot.config.scaled(kWebDirectMs);
            motion.moveToFraction(ServoId::Translation, 0.0f, ms);
            motion.moveToFraction(ServoId::Rotation,    0.0f, ms);
            motion.moveToFraction(ServoId::HeadPan,     0.0f, ms);
            motion.moveToFraction(ServoId::HeadTilt,    0.0f, ms);
        } else if (!strcmp(name, "eyeCycle")) {
            eyes.showTestImage();
        } else if (!strcmp(name, "eyeIdentify")) {
            eyes.showIdentify();
        } else {
            Serial.printf("WebControl: unknown action '%s'\n", name);
        }
        sendState(client);
        return;
    }
    if (!strcmp(cmd, "head")) {
        float pan  = doc["pan"]  | 0.0f;
        float tilt = doc["tilt"] | 0.0f;
        pan  = constrain(pan,  -1.0f, 1.0f);
        tilt = constrain(tilt, -1.0f, 1.0f);
        uint32_t ms = robot.config.scaled(kWebDirectMs);
        motion.moveToFraction(ServoId::HeadPan,  pan,  ms);
        motion.moveToFraction(ServoId::HeadTilt, tilt, ms);
        // No sendState here — the head pad streams events during drag and
        // we don't want to flood every client with a state echo per sample.
        return;
    }
    if (!strcmp(cmd, "expression")) {
        const char* name = doc["name"] | "";
        for (int i = 0; i < static_cast<int>(RobotFace::Expression::_Count); i++) {
            auto e = static_cast<RobotFace::Expression>(i);
            if (!strcmp(name, RobotFace::expressionName(e))) {
                face.setExpression(e);
                eyes.setExpression(e);
                break;
            }
        }
        sendState(client);
        return;
    }
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
    doc["expression"] = RobotFace::expressionName(face.getExpression());
    doc["idle"]       = robot.isIdle();
    JsonObject p      = doc["power"].to<JsonObject>();
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
