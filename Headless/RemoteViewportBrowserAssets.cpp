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
          <video id="viewport-video" autoplay playsinline muted></video>
          <div id="viewport-overlay" class="viewport-overlay">
            Waiting for WebRTC session...
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
  max-height: 70vh;
  aspect-ratio: 16 / 9;
}

#viewport-video {
  display: block;
  width: 100%;
  height: 100%;
  max-width: 100%;
  min-height: 0;
  max-height: 70vh;
  object-fit: contain;
  background: rgba(4, 8, 16, 0.92);
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
  peerConnection: null,
  reliableChannel: null,
  unreliableChannel: null,
  pendingLocalIceCandidates: [],
  remoteDescriptionApplied: false,
  statusPollHandle: null,
  icePollHandle: null,
  keepPollingStatus: false,
  isLooking: false,
  keys: new Set(),
  pointerLocked: false,
  cursor: { x: 0, y: 0 },
  pendingLookDelta: { x: 0, y: 0 },
  lastFrameIndex: 0,
  webrtcStatus: null,
  signalingInFlight: false,
  connectionGeneration: 0,
  inputFrameHandle: null
};

const statusPill = document.getElementById('status-pill');
const frameMeta = document.getElementById('frame-meta');
const viewportVideo = document.getElementById('viewport-video');
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

function updateViewportMetrics() {
  const width = viewportVideo.videoWidth;
  const height = viewportVideo.videoHeight;
  if (!width || !height) {
    return;
  }
  frameMeta.textContent = `${width}x${height}`;
  viewportShell.style.aspectRatio = `${width} / ${height}`;
}

function appendError(context, error) {
  const message = error && typeof error === 'object' && 'message' in error
    ? error.message
    : String(error);
  appendLog(`${context}: ${message}`);
}

function currentGeneration() {
  return state.connectionGeneration;
}

function channelOpen(channel) {
  return channel && channel.readyState === 'open';
}

function canSendReliably() {
  return channelOpen(state.reliableChannel);
}

function canSendUnreliably() {
  return channelOpen(state.unreliableChannel);
}

async function flushPendingLocalIceCandidates(expectedGeneration) {
  if (!state.peerConnection || expectedGeneration !== currentGeneration()) {
    return;
  }
  if (!state.remoteDescriptionApplied || state.pendingLocalIceCandidates.length === 0) {
    return;
  }

  const pending = state.pendingLocalIceCandidates.splice(0, state.pendingLocalIceCandidates.length);
  for (const candidate of pending) {
    try {
      await postJson('/webrtc/ice-candidate', candidate);
      appendLog({ type: 'local_ice_uploaded', candidate: candidate.candidate });
    } catch (error) {
      appendError('local ICE upload failed', error);
      state.pendingLocalIceCandidates.unshift(candidate);
      break;
    }
  }
}

async function postJson(url, payload) {
  const response = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(payload)
  });

  const text = await response.text();
  let data = null;
  if (text.length > 0) {
    try {
      data = JSON.parse(text);
    } catch (error) {
      data = { type: 'raw', body: text };
    }
  }

  if (!response.ok) {
    const detail = data && data.detail ? data.detail
      : data && data.message ? data.message
      : `${response.status} ${response.statusText}`;
    throw new Error(detail);
  }

  return data;
}

async function postText(url, text) {
  const response = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'text/plain; charset=utf-8'
    },
    body: text
  });

  const raw = await response.text();
  let data = null;
  if (raw.length > 0) {
    try {
      data = JSON.parse(raw);
    } catch (error) {
      data = { type: 'raw', body: raw };
    }
  }

  if (!response.ok) {
    const detail = data && data.detail ? data.detail
      : data && data.message ? data.message
      : `${response.status} ${response.statusText}`;
    throw new Error(detail);
  }

  return data;
}

async function sendCommand(payload, preferredChannel = 'reliable') {
  const serialized = JSON.stringify(payload);
  if (preferredChannel === 'unreliable' && canSendUnreliably()) {
    state.unreliableChannel.send(serialized);
    return true;
  }
  if (canSendReliably()) {
    state.reliableChannel.send(serialized);
    return true;
  }

  try {
    await postJson('/command', payload);
    return true;
  } catch (error) {
    appendError('command failed', error);
    return false;
  }
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
  void sendCommand({
    type: 'set_look_active',
    isLooking: state.isLooking,
    cursorPosition: [state.cursor.x, state.cursor.y]
  }, 'reliable');
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

function sendViewportInputFrame() {
  const lookChanged = applyPendingLook();
  const [x, y, z] = movementVector();
  if (!lookChanged && x === 0 && y === 0 && z === 0) {
    return false;
  }

  void sendCommand({
    type: 'update_viewport_camera',
    worldMovement: [x, y, z],
    cursorPosition: [state.cursor.x, state.cursor.y]
  }, 'unreliable');
  return true;
}

function pumpViewportInput() {
  sendViewportInputFrame();
  state.inputFrameHandle = window.requestAnimationFrame(pumpViewportInput);
}

function startViewportInputPump() {
  if (state.inputFrameHandle !== null) {
    return;
  }
  state.inputFrameHandle = window.requestAnimationFrame(pumpViewportInput);
}

function stopViewportInputPump() {
  if (state.inputFrameHandle === null) {
    return;
  }
  window.cancelAnimationFrame(state.inputFrameHandle);
  state.inputFrameHandle = null;
}

function updateFrameMetaFromStatus(status) {
  if (!status || !status.video) {
    frameMeta.textContent = 'No WebRTC status yet';
    return;
  }

  const parts = [`Codec ${status.video.codec}`];
  if (status.video.lastFrameIndex !== null && status.video.lastFrameIndex !== undefined) {
    parts.push(`Frame ${status.video.lastFrameIndex}`);
  }
  if (status.video.pendingPacketCount !== undefined) {
    parts.push(`Queued ${status.video.pendingPacketCount}`);
  }
  if (status.video.droppedPacketCount !== undefined && status.video.droppedPacketCount > 0) {
    parts.push(`Dropped ${status.video.droppedPacketCount}`);
  }
  frameMeta.textContent = parts.join(' - ');
}

function updateStatusFromWebRtc(status) {
  state.webrtcStatus = status;
  updateFrameMetaFromStatus(status);

  if (!status.enabled) {
    setStatus('error', 'WebRTC Disabled');
    viewportOverlay.textContent = status.detail || 'WebRTC support is disabled in this build.';
    viewportOverlay.style.display = 'grid';
    return;
  }

  if (!status.available) {
    setStatus('idle', 'Awaiting Backend');
    viewportOverlay.textContent = status.detail || 'Waiting for native WebRTC backend.';
    viewportOverlay.style.display = 'grid';
    return;
  }

  if (state.peerConnection && state.peerConnection.connectionState === 'connected') {
    setStatus('connected', 'Streaming');
    return;
  }

  setStatus('idle', 'Negotiating');
  viewportOverlay.textContent = status.detail || 'Negotiating WebRTC session...';
  viewportOverlay.style.display = 'grid';
}

function stopPolling() {
  if (state.statusPollHandle) {
    clearInterval(state.statusPollHandle);
    state.statusPollHandle = null;
  }
  if (state.icePollHandle) {
    clearInterval(state.icePollHandle);
    state.icePollHandle = null;
  }
  state.keepPollingStatus = false;
}

async function notifyServerSessionClosed(reason) {
  try {
    const response = await postText('/webrtc/close', reason);
    appendLog({ type: 'webrtc_close_ack', reason, response });
  } catch (error) {
    appendError('server close notification failed', error);
  }
}

async function destroyPeerConnection(reason = 'client_reset', notifyServer = false) {
  stopPolling();
  stopViewportInputPump();
  if (state.reliableChannel) {
    state.reliableChannel.close();
    state.reliableChannel = null;
  }
  if (state.unreliableChannel) {
    state.unreliableChannel.close();
    state.unreliableChannel = null;
  }
  if (state.peerConnection) {
    state.peerConnection.close();
    state.peerConnection = null;
  }
  viewportVideo.srcObject = null;
  if (notifyServer) {
    await notifyServerSessionClosed(reason);
  }
}

async function pollWebRtcStatus(expectedGeneration) {
  if (!state.keepPollingStatus || expectedGeneration !== currentGeneration()) {
    return;
  }
  try {
    const status = await fetch('/webrtc', { cache: 'no-store' }).then((response) => response.json());
    if (!state.keepPollingStatus || expectedGeneration !== currentGeneration()) {
      return;
    }
    updateStatusFromWebRtc(status);
  } catch (error) {
    appendError('status poll failed', error);
    setStatus('error', 'Status Error');
  }
}

async function pollIceCandidates(expectedGeneration) {
  if (!state.peerConnection || expectedGeneration !== currentGeneration()) {
    return;
  }

  try {
    const response = await fetch('/webrtc/ice-candidates', { cache: 'no-store' });
    const payload = await response.json();
    if (!state.peerConnection || expectedGeneration !== currentGeneration()) {
      return;
    }
    if (!payload.candidates || payload.candidates.length === 0) {
      return;
    }

    for (const candidate of payload.candidates) {
      await state.peerConnection.addIceCandidate(candidate);
      appendLog({ type: 'remote_ice_candidate', candidate });
    }
  } catch (error) {
    appendError('ICE poll failed', error);
  }
}

function wireDataChannel(channel, label) {
  channel.addEventListener('open', () => {
    appendLog({ type: 'data_channel_open', label });
    if (label === 'editor-events') {
      setStatus('connected', 'Control Ready');
    }
  });
  channel.addEventListener('close', () => {
    appendLog({ type: 'data_channel_closed', label });
  });
  channel.addEventListener('error', (event) => {
    appendLog({ type: 'data_channel_error', label, detail: String(event.type || 'error') });
  });
  channel.addEventListener('message', (event) => {
    try {
      appendLog(JSON.parse(event.data));
    } catch (error) {
      appendLog({ type: 'data_channel_message', label, body: event.data });
    }
  });
}

viewportVideo.addEventListener('loadedmetadata', () => {
  updateViewportMetrics();
  appendLog({ type: 'video_loadedmetadata', width: viewportVideo.videoWidth, height: viewportVideo.videoHeight });
});

viewportVideo.addEventListener('resize', () => {
  updateViewportMetrics();
  appendLog({ type: 'video_resize', width: viewportVideo.videoWidth, height: viewportVideo.videoHeight });
});

async function connect() {
  const generation = currentGeneration() + 1;
  state.connectionGeneration = generation;
  await destroyPeerConnection('reconnect', true);
  state.signalingInFlight = true;
  state.keepPollingStatus = true;
  startViewportInputPump();
  state.pendingLocalIceCandidates = [];
  state.remoteDescriptionApplied = false;

  setStatus('idle', 'Connecting');
  viewportOverlay.textContent = 'Creating browser WebRTC offer...';
  viewportOverlay.style.display = 'grid';

  if (!window.RTCPeerConnection) {
    setStatus('error', 'Unsupported');
    viewportOverlay.textContent = 'This browser does not support RTCPeerConnection.';
    state.signalingInFlight = false;
    return;
  }

  const peer = new RTCPeerConnection({
    bundlePolicy: 'max-bundle',
    rtcpMuxPolicy: 'require'
  });
  state.peerConnection = peer;

  peer.addEventListener('connectionstatechange', () => {
    if (generation !== currentGeneration()) {
      return;
    }
    appendLog({ type: 'connection_state', state: peer.connectionState });
    if (peer.connectionState === 'connected') {
      setStatus('connected', 'Streaming');
      viewportOverlay.style.display = 'none';
    } else if (peer.connectionState === 'closed') {
      setStatus('idle', 'Closed');
      viewportOverlay.textContent = 'Peer connection closed.';
      viewportOverlay.style.display = 'grid';
    } else if (peer.connectionState === 'failed' || peer.connectionState === 'disconnected') {
      setStatus('error', 'Peer Failed');
      viewportOverlay.textContent = `Peer connection ${peer.connectionState}.`;
      viewportOverlay.style.display = 'grid';
    }
  });

  peer.addEventListener('signalingstatechange', () => {
    if (generation !== currentGeneration()) {
      return;
    }
    appendLog({ type: 'signaling_state', state: peer.signalingState });
  });

  peer.addEventListener('iceconnectionstatechange', () => {
    if (generation !== currentGeneration()) {
      return;
    }
    appendLog({ type: 'ice_connection_state', state: peer.iceConnectionState });
    if (peer.iceConnectionState === 'failed') {
      viewportOverlay.textContent = 'ICE failed. Reconnect after the server is ready.';
      viewportOverlay.style.display = 'grid';
    }
  });

  peer.addEventListener('track', (event) => {
    if (generation !== currentGeneration()) {
      return;
    }
    appendLog({ type: 'remote_track', kind: event.track.kind });
    const [stream] = event.streams;
    if (stream) {
      viewportVideo.srcObject = stream;
      viewportOverlay.style.display = 'none';
    }
  });

  peer.addTransceiver('video', {
    direction: 'recvonly'
  });
  appendLog({ type: 'transceiver_added', kind: 'video', direction: 'recvonly' });

  peer.addEventListener('icecandidate', async (event) => {
    if (generation !== currentGeneration()) {
      return;
    }
    if (!event.candidate) {
      appendLog({ type: 'ice_gathering_complete' });
      return;
    }

    appendLog({ type: 'local_ice_candidate', candidate: event.candidate.candidate });
    if (!event.candidate.candidate) {
      return;
    }

    state.pendingLocalIceCandidates.push({
      candidate: event.candidate.candidate,
      sdpMid: event.candidate.sdpMid,
      sdpMLineIndex: event.candidate.sdpMLineIndex
    });
    if (state.remoteDescriptionApplied) {
      await flushPendingLocalIceCandidates(generation);
    }
  });

  state.reliableChannel = peer.createDataChannel('editor-events', {
    ordered: true
  });
  wireDataChannel(state.reliableChannel, 'editor-events');

  state.unreliableChannel = peer.createDataChannel('viewport-input', {
    ordered: false,
    maxRetransmits: 0
  });
  wireDataChannel(state.unreliableChannel, 'viewport-input');

  try {
    appendLog({ type: 'creating_offer' });
    const offer = await peer.createOffer();
    appendLog({ type: 'offer_created', typeName: offer.type, sdpLength: offer.sdp ? offer.sdp.length : 0 });
    await peer.setLocalDescription(offer);
    appendLog({ type: 'local_description_set', signalingState: peer.signalingState });

    const answer = await postJson('/webrtc/offer', {
      type: offer.type,
      sdp: offer.sdp
    });
    if (generation !== currentGeneration()) {
      return;
    }
    appendLog({ type: 'offer_response', body: answer });

    if (!answer || answer.type !== 'answer' || !answer.sdp) {
      throw new Error(answer && answer.detail
        ? answer.detail
        : 'Server did not return a valid WebRTC answer.');
    }

    await peer.setRemoteDescription(answer);
    state.remoteDescriptionApplied = true;
    appendLog({ type: 'remote_description_set', signalingState: peer.signalingState });
    await flushPendingLocalIceCandidates(generation);
    setStatus('idle', 'Awaiting Media');
    viewportOverlay.textContent = 'Offer accepted. Waiting for remote media...';
  } catch (error) {
    if (generation !== currentGeneration()) {
      return;
    }
    appendError('WebRTC negotiation failed', error);
    setStatus('error', 'Negotiation Failed');
    viewportOverlay.textContent = String(error.message || error);
    viewportOverlay.style.display = 'grid';
  } finally {
    state.signalingInFlight = false;
  }

  state.statusPollHandle = window.setInterval(() => {
    void pollWebRtcStatus(generation);
  }, 1500);
  state.icePollHandle = window.setInterval(() => {
    void pollIceCandidates(generation);
  }, 1000);

  void pollWebRtcStatus(generation);
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

window.addEventListener('beforeunload', () => {
  void destroyPeerConnection('page_unload', true);
});

document.addEventListener('mousemove', (event) => {
  if (!state.pointerLocked) {
    return;
  }
  state.pendingLookDelta.x += event.movementX;
  state.pendingLookDelta.y += event.movementY;
  if (state.isLooking) {
    sendViewportInputFrame();
  }
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
