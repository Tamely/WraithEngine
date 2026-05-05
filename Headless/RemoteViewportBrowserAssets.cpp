#include "RemoteViewportBrowserAssets.h"

namespace Axiom {
namespace {
constexpr std::string_view HtmlPage = R"html(<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Wraith Remote Viewport</title>
    <link rel="stylesheet" href="/style.css" />
  </head>
  <body>
    <main class="shell">
      <section class="panel hero">
        <p class="eyebrow">Wraith Engine</p>
        <h1>Remote Viewport Prototype</h1>
        <p class="summary">
          Connects to the local headless authoritative session, shows the latest
          viewport frame, and forwards camera input over the browser boundary.
        </p>
        <div class="status-row">
          <span id="status-pill" class="pill idle">Connecting</span>
          <span id="frame-meta" class="meta">No frames yet</span>
        </div>
      </section>
      <section class="panel viewport-panel">
        <div class="viewport-toolbar">
          <button id="connect-button" type="button">Reconnect Stream</button>
          <button id="look-button" type="button">Toggle Look</button>
          <span class="meta">WASD to move, drag or pointer-lock to look</span>
        </div>
        <div id="viewport-shell" class="viewport-shell">
          <img id="viewport-image" alt="Remote viewport frame" />
          <div id="viewport-overlay" class="viewport-overlay">
            Waiting for stream...
          </div>
        </div>
      </section>
      <section class="panel log-panel">
        <div class="log-header">
          <h2>Session Log</h2>
          <button id="clear-log-button" type="button">Clear</button>
        </div>
        <pre id="event-log" class="event-log"></pre>
      </section>
    </main>
    <script src="/app.js"></script>
  </body>
</html>)html";

constexpr std::string_view Stylesheet = R"css(:root {
  color-scheme: dark;
  --bg: #0d1117;
  --panel: #111827;
  --panel-alt: #182235;
  --text: #ebf2ff;
  --muted: #9fb0cb;
  --accent: #4ecdc4;
  --accent-strong: #ffb703;
  --danger: #ff6b6b;
}

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  min-height: 100vh;
  font-family: "Segoe UI", "Trebuchet MS", sans-serif;
  background:
    radial-gradient(circle at top, rgba(78, 205, 196, 0.18), transparent 35%),
    linear-gradient(160deg, #091018 0%, #0d1117 48%, #131d2f 100%);
  color: var(--text);
}

.shell {
  width: min(1200px, calc(100% - 32px));
  margin: 24px auto 40px;
  display: grid;
  gap: 18px;
}

.panel {
  background: rgba(17, 24, 39, 0.86);
  border: 1px solid rgba(159, 176, 203, 0.2);
  border-radius: 18px;
  box-shadow: 0 18px 50px rgba(0, 0, 0, 0.28);
  backdrop-filter: blur(12px);
}

.hero {
  padding: 24px;
}

.eyebrow {
  margin: 0 0 8px;
  color: var(--accent);
  font-size: 0.82rem;
  text-transform: uppercase;
  letter-spacing: 0.18em;
}

h1 {
  margin: 0;
  font-size: clamp(2rem, 4vw, 3.2rem);
}

.summary {
  max-width: 64ch;
  color: var(--muted);
  line-height: 1.5;
}

.status-row,
.viewport-toolbar,
.log-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex-wrap: wrap;
}

.pill {
  display: inline-flex;
  align-items: center;
  border-radius: 999px;
  padding: 8px 14px;
  font-weight: 700;
  letter-spacing: 0.03em;
}

.pill.idle {
  background: rgba(255, 183, 3, 0.18);
  color: #ffd166;
}

.pill.connected {
  background: rgba(78, 205, 196, 0.16);
  color: #81ecec;
}

.pill.error {
  background: rgba(255, 107, 107, 0.18);
  color: #ff9b9b;
}

.meta {
  color: var(--muted);
  font-size: 0.92rem;
}

.viewport-panel,
.log-panel {
  padding: 18px;
}

button {
  border: 0;
  border-radius: 999px;
  padding: 11px 16px;
  background: linear-gradient(135deg, var(--accent), #2a9d8f);
  color: #03111d;
  font-weight: 700;
  cursor: pointer;
}

button:hover {
  filter: brightness(1.08);
}

.viewport-shell {
  position: relative;
  margin-top: 16px;
  overflow: hidden;
  border-radius: 14px;
  border: 1px solid rgba(159, 176, 203, 0.18);
  background:
    linear-gradient(135deg, rgba(78, 205, 196, 0.08), transparent 40%),
    rgba(6, 10, 18, 0.9);
  min-height: 360px;
}

#viewport-image {
  display: block;
  width: 100%;
  max-width: 100%;
}

.viewport-overlay {
  position: absolute;
  inset: 0;
  display: grid;
  place-items: center;
  color: var(--muted);
  font-size: 1rem;
  background: rgba(3, 9, 16, 0.46);
}

.event-log {
  margin: 16px 0 0;
  padding: 16px;
  border-radius: 12px;
  min-height: 200px;
  max-height: 280px;
  overflow: auto;
  background: rgba(7, 12, 20, 0.9);
  color: #c6d4ef;
  font-size: 0.9rem;
}

@media (max-width: 720px) {
  .shell {
    width: min(100% - 20px, 1200px);
    margin-top: 12px;
  }

  .hero,
  .viewport-panel,
  .log-panel {
    padding: 16px;
  }

  .viewport-shell {
    min-height: 220px;
  }
})css";

constexpr std::string_view BrowserScript = R"js(const state = {
  socket: null,
  socketUrl: `${window.location.protocol === 'https:' ? 'wss' : 'ws'}://${window.location.host}/ws`,
  isLooking: false,
  keys: new Set(),
  pointerLocked: false,
  cursor: { x: 0, y: 0 },
  pendingLookDelta: { x: 0, y: 0 },
  pendingFrameMeta: null,
  currentFrameUrl: null,
  lastFrameIndex: 0
};

const statusPill = document.getElementById('status-pill');
const frameMeta = document.getElementById('frame-meta');
const viewportImage = document.getElementById('viewport-image');
const viewportOverlay = document.getElementById('viewport-overlay');
const eventLog = document.getElementById('event-log');
const connectButton = document.getElementById('connect-button');
const lookButton = document.getElementById('look-button');
const clearLogButton = document.getElementById('clear-log-button');
const viewportShell = document.getElementById('viewport-shell');

function appendLog(value) {
  const line = typeof value === 'string' ? value : JSON.stringify(value);
  eventLog.textContent = `${line}\n${eventLog.textContent}`.slice(0, 12000);
}

function setStatus(kind, text) {
  statusPill.className = `pill ${kind}`;
  statusPill.textContent = text;
}

function canSend() {
  return state.socket && state.socket.readyState === WebSocket.OPEN;
}

function sendCommand(payload) {
  if (!canSend()) {
    return;
  }
  state.socket.send(JSON.stringify(payload));
}

function updateLookButton() {
  lookButton.textContent = state.isLooking ? 'Disable Look' : 'Toggle Look';
}

function setLookEnabled(nextValue) {
  if (state.isLooking === nextValue) {
    return;
  }
  state.isLooking = nextValue;
  updateLookButton();
  sendCommand({
    type: 'set_look_active',
    isLooking: state.isLooking,
    cursorPosition: [state.cursor.x, state.cursor.y]
  });
}

function movementVector() {
  const forward = (state.keys.has('KeyW') ? 1 : 0) - (state.keys.has('KeyS') ? 1 : 0);
  const strafe = (state.keys.has('KeyD') ? 1 : 0) - (state.keys.has('KeyA') ? 1 : 0);
  const lift = (state.keys.has('Space') ? 1 : 0) -
    ((state.keys.has('ShiftLeft') || state.keys.has('ShiftRight')) ? 1 : 0);
  return [strafe * 0.08, lift * 0.08, forward * 0.08];
}

function applyPendingLook() {
  if (!state.isLooking) {
    state.pendingLookDelta.x = 0;
    state.pendingLookDelta.y = 0;
    return false;
  }

  if (state.pendingLookDelta.x === 0 && state.pendingLookDelta.y === 0) {
    return false;
  }

  state.cursor.x += state.pendingLookDelta.x;
  state.cursor.y += state.pendingLookDelta.y;
  state.pendingLookDelta.x = 0;
  state.pendingLookDelta.y = 0;
  return true;
}

function connect() {
  if (state.socket) {
    state.socket.close();
  }

  setStatus('idle', 'Connecting');
  viewportOverlay.textContent = 'Connecting to remote viewport...';
  viewportOverlay.style.display = 'grid';

  const socket = new WebSocket(state.socketUrl);
  socket.binaryType = 'blob';
  state.socket = socket;

  socket.addEventListener('open', () => {
    setStatus('connected', 'Connected');
  });

  socket.addEventListener('message', async (event) => {
    if (typeof event.data === 'string') {
      const message = JSON.parse(event.data);
      appendLog(message);

      switch (message.type) {
        case 'ready':
          frameMeta.textContent = `${message.width} x ${message.height}`;
          break;
        case 'connected':
          setStatus('connected', 'Connected');
          viewportOverlay.textContent = 'Waiting for the next frame...';
          break;
        case 'frame':
          state.pendingFrameMeta = message;
          break;
        case 'error':
          setStatus('error', 'Server Error');
          viewportOverlay.textContent = message.message;
          viewportOverlay.style.display = 'grid';
          break;
        case 'shutdown':
          setStatus('idle', 'Server Stopped');
          viewportOverlay.textContent = 'Server shut down.';
          viewportOverlay.style.display = 'grid';
          break;
        default:
          break;
      }
      return;
    }

    if (state.currentFrameUrl) {
      URL.revokeObjectURL(state.currentFrameUrl);
    }
    state.currentFrameUrl = URL.createObjectURL(event.data);
    viewportImage.src = state.currentFrameUrl;
    viewportOverlay.style.display = 'none';

    if (state.pendingFrameMeta) {
      state.lastFrameIndex = state.pendingFrameMeta.frameIndex;
      frameMeta.textContent = `Frame ${state.pendingFrameMeta.frameIndex} - ${state.pendingFrameMeta.width} x ${state.pendingFrameMeta.height}`;
      state.pendingFrameMeta = null;
    }
  });

  socket.addEventListener('close', () => {
    setStatus('error', 'Disconnected');
    viewportOverlay.textContent = 'Stream disconnected. Reconnect when the server is ready.';
    viewportOverlay.style.display = 'grid';
  });

  socket.addEventListener('error', () => {
    setStatus('error', 'Socket Error');
  });
}

connectButton.addEventListener('click', connect);
lookButton.addEventListener('click', () => setLookEnabled(!state.isLooking));
clearLogButton.addEventListener('click', () => {
  eventLog.textContent = '';
});

viewportShell.addEventListener('click', async () => {
  try {
    await viewportShell.requestPointerLock();
  } catch (error) {
    appendLog(`pointer lock failed: ${error}`);
  }
});

document.addEventListener('pointerlockchange', () => {
  state.pointerLocked = document.pointerLockElement === viewportShell;
  if (!state.pointerLocked && state.isLooking) {
    setLookEnabled(false);
  } else if (state.pointerLocked && !state.isLooking) {
    setLookEnabled(true);
  }
});

document.addEventListener('mousemove', (event) => {
  if (!state.pointerLocked) {
    return;
  }
  state.pendingLookDelta.x += event.movementX;
  state.pendingLookDelta.y += event.movementY;
});

document.addEventListener('keydown', (event) => {
  if (event.code === 'Space' ||
      event.code === 'ShiftLeft' ||
      event.code === 'ShiftRight' ||
      event.code === 'KeyW' ||
      event.code === 'KeyA' ||
      event.code === 'KeyS' ||
      event.code === 'KeyD') {
    event.preventDefault();
  }
  state.keys.add(event.code);
});

document.addEventListener('keyup', (event) => {
  if (event.code === 'Space' ||
      event.code === 'ShiftLeft' ||
      event.code === 'ShiftRight' ||
      event.code === 'KeyW' ||
      event.code === 'KeyA' ||
      event.code === 'KeyS' ||
      event.code === 'KeyD') {
    event.preventDefault();
  }
  state.keys.delete(event.code);
});

setInterval(() => {
  if (!canSend()) {
    return;
  }

  const lookChanged = applyPendingLook();
  const [x, y, z] = movementVector();
  if (!lookChanged && x === 0 && y === 0 && z === 0) {
    return;
  }

  sendCommand({
    type: 'update_viewport_camera',
    worldMovement: [x, y, z],
    cursorPosition: [state.cursor.x, state.cursor.y]
  });
}, 16);

updateLookButton();
connect();
)js";
} // namespace

bool TryGetRemoteViewportBrowserAsset(std::string_view Route,
                                      RemoteViewportBrowserAsset &Asset) {
  if (Route == "/") {
    Asset = {
        .ContentType = "text/html; charset=utf-8",
        .Body = HtmlPage,
    };
    return true;
  }
  if (Route == "/app.js") {
    Asset = {
        .ContentType = "application/javascript; charset=utf-8",
        .Body = BrowserScript,
    };
    return true;
  }
  if (Route == "/style.css") {
    Asset = {
        .ContentType = "text/css; charset=utf-8",
        .Body = Stylesheet,
    };
    return true;
  }
  return false;
}
} // namespace Axiom
