# Headless Axiom Session Prototype

`AxiomHeadless` is a prototype executable that runs the Axiom editor session without a GLFW window, accepts newline-delimited JSON commands on `stdin`, and writes captured PNG frames plus NDJSON status/events.

`AxiomRemoteViewportDevClient` is a companion dev executable that exercises the same authoritative session through the new transport seam and writes the frames it receives as a transport subscriber.

`AxiomRemoteViewportServer` is now the primary remote viewport prototype backend. It runs the authoritative headless session, exposes HTTP/WebRTC endpoints for the browser editor, and keeps a JPEG `/frame` path available only as a diagnostics and fallback surface.

The current slice includes a macOS-first H.264 path that is now wired into the native WebRTC sender. Encoded packets are still exposed through diagnostics endpoints, but the browser viewport now consumes the WebRTC video track rather than the older JPEG push path during normal use.

## Current Status

- Status: working prototype
- Verified on Windows as of 2026-05-05
- Builds on macOS as of 2026-05-05
- Runtime validation on macOS requires a Vulkan/MoltenVK-capable environment with Metal available
- This subphase is complete for the runtime-side seam restoration work
- `AxiomHeadless` is a command-driven authoritative runtime, not a full editor client
- viewport frames are emitted through the engine/session frame-output seam and then written to PNG by the prototype executable
- `ISessionTransport` now exists as the engine-facing remote boundary
- `AxiomSessionEndpoint` is the first in-process transport implementation
- `AxiomRemoteViewportDevClient` is a dev harness for transport-delivered frames/events, not a browser client
- `AxiomRemoteViewportServer` is the current browser-facing backend for remote viewport work
- the current server/browser slice has moved from SSE plus HTTP image polling to WebRTC video plus data channels, with JPEG retained only for diagnostics and fallback access
- encoded video packet delivery now exists as an additive transport seam beside raw viewport frame delivery
- `IVideoEncoder` now exists as the engine-owned video encode boundary
- a macOS `VideoToolbox` H.264 encoder path now exists for headless remote-viewport bring-up
- `AxiomRemoteViewportServer` now exposes H.264 diagnostics through separate `/h264` and `/h264/metadata` endpoints while preserving a JPEG `/frame` fallback
- the main-loop throttle in the headless remote viewport server has been removed so the runtime can tick at full cadence
- the server now skips duplicate JPEG encode/broadcast work while WebRTC is actively connected
- the macOS `VideoToolbox` encoder is now tuned for lower latency by avoiding per-frame synchronous completion, tightening bitrate/rate limits, and shortening the keyframe interval
- the native WebRTC sender now prefers the freshest available H.264 packet instead of rewinding to older packets during normal latest-only delivery
- the browser client now pumps camera/input updates on `requestAnimationFrame` and flushes pointer-lock look input immediately instead of batching on a fixed timer
- the current stream no longer has the severe FPS collapse seen in the older prototype, but there is still roughly half a second of residual input latency to investigate later
- a root-level `EditorFrontend` workspace now hosts the primary browser editor shell using Next.js, React, and Tailwind CSS
- `EditorFrontend` includes the docked editor layout, menu bar, toolbar, outliner, details panel, content browser, and the active WebRTC viewport client in `components/engine/viewport.tsx`
- the old inline localhost:8080 page has been retired; the server now focuses on backend/session and diagnostics endpoints
- sandboxed validation on macOS can hide the availability of VideoToolbox H.264 encoders; authoritative H.264 validation should be done outside the sandbox on a Vulkan/MoltenVK-capable machine

## CLI

Required arguments:

- `--output-dir <path>`

Optional arguments:

- `--width <pixels>` default `1600`
- `--height <pixels>` default `900`

Example:

```powershell
.\AxiomHeadless.exe --output-dir D:\Temp\axiom-headless --width 1280 --height 720
```

Dev-client example:

```powershell
.\AxiomRemoteViewportDevClient.exe --output-dir D:\Temp\axiom-remote-dev --width 1280 --height 720
```

Remote viewport server example:

```sh
./AxiomRemoteViewportServer --host 127.0.0.1 --port 8080 --width 1280 --height 720 --jpeg-quality 75
```

Then start the browser editor:

```sh
cd EditorFrontend
NEXT_PUBLIC_AXIOM_SERVER_ORIGIN=http://127.0.0.1:8080 pnpm dev
```

Then open the local Next.js URL shown by the dev server, typically [http://127.0.0.1:3000](http://127.0.0.1:3000).

Tested one-shot example:

```powershell
@'
{"type":"load_startup_scene"}
{"type":"render_frame"}
{"type":"quit"}
'@ | .\AxiomHeadless.exe --output-dir D:\Temp\axiom-headless --width 1280 --height 720
```

## Output Messages

The process writes one JSON object per line to `stdout`.

- `ready`
- `event`
- `frame`
- `error`
- `shutdown`

Example `ready` message:

```json
{"type":"ready","width":1280,"height":720}
```

## Input Commands

Supported command types:

- `load_startup_scene`
- `set_view_mode`
- `set_look_active`
- `update_viewport_camera`
- `render_frame`
- `quit`

Example session:

```json
{"type":"load_startup_scene"}
{"type":"set_view_mode","viewMode":"wireframe"}
{"type":"set_look_active","isLooking":true,"cursorPosition":[100.0,120.0]}
{"type":"update_viewport_camera","worldMovement":[0.0,0.0,-0.25],"cursorPosition":[108.0,116.0]}
{"type":"render_frame"}
{"type":"quit"}
```

Supported view modes:

- `lit`
- `unlit`
- `wireframe`

## Frame Files

Each `render_frame` command writes one numbered PNG in `--output-dir`:

- `frame_000001.png`
- `frame_000002.png`

The corresponding `frame` message includes the file path and output dimensions.

For the tested one-shot example above, the expected first output file is:

- `frame_000001.png`

## Ordering Rules

- `render_frame` before `load_startup_scene` returns an `error`
- repeating `load_startup_scene` returns an `error`
- malformed JSON or unsupported command types return an `error`
- failing to create the output directory or write a PNG returns an `error`

## Scope Boundary

This prototype proves that:

- the authoritative editor session can run without a visible GLFW window
- the startup scene can be loaded and rendered offscreen
- the process can emit status/events and write captured viewport frames
- the transport seam can deliver authoritative events and viewport frames to a dev subscriber
- the same seam can now deliver encoded video packets in addition to raw viewport frames
- remote-style command submission can reuse the same session authority path as the local adapter
- a browser can connect to the headless authoritative session over a real network boundary and drive the existing viewport camera commands
- the browser client can now receive the authoritative viewport over a WebRTC H.264 video track while using data channels for control/input traffic
- the headless server can now expose the latest H.264 access unit and packet metadata for diagnostics while keeping `/frame` available as a non-primary fallback path
- the major FPS bottlenecks in the remote viewport prototype have been removed through server-loop, duplicate-work, encoder, and input-pump latency fixes

This prototype does not yet provide:

- the low-latency input response expected from the final remote editor target
- a production-ready remote viewer/editor client
- a replacement for the current local windowed editor executable

## Next Priority

The next remote-viewport milestone should prioritize:

- deeper WebRTC sender and browser playout-delay tuning to reduce the remaining input latency
- continued hardening of the `EditorFrontend/components/engine/viewport.tsx` client as the primary browser UI
- better editor-shell integration around session lifecycle, transport health, and future collaboration surfaces
