# Headless Axiom Session Prototype

`AxiomHeadless` is a prototype executable that runs the Axiom editor session without a GLFW window, accepts newline-delimited JSON commands on `stdin`, and writes captured PNG frames plus NDJSON status/events.

`AxiomRemoteViewportDevClient` is a companion dev executable that exercises the same authoritative session through the new transport seam and writes the frames it receives as a transport subscriber.

`AxiomRemoteViewportServer` is now the primary remote viewport prototype. It runs the authoritative headless session, serves a browser client over HTTP, streams JSON control/events over WebSocket, and pushes binary JPEG viewport frames over that same WebSocket connection.

## Current Status

- Status: working prototype
- Verified on Windows as of 2026-05-05
- This subphase is complete for the runtime-side seam restoration work
- `AxiomHeadless` is a command-driven authoritative runtime, not a full editor client
- viewport frames are emitted through the engine/session frame-output seam and then written to PNG by the prototype executable
- `ISessionTransport` now exists as the engine-facing remote boundary
- `AxiomSessionEndpoint` is the first in-process transport implementation
- `AxiomRemoteViewportDevClient` is a dev harness for transport-delivered frames/events, not a browser client
- `AxiomRemoteViewportServer` is the current browser-facing demo path for remote viewport work
- the current server/browser slice has moved from SSE plus HTTP image polling to WebSocket plus JPEG streaming
- the current stream is meaningfully better than the original PNG-polling prototype, but is still too choppy for the target remote-editor experience
- WebRTC plus H.264 is now the near-term transport priority rather than a later optional refinement
- the long-lived browser editor UI should move into a root-level `EditorFrontend` workspace using React, Next.js, and Tailwind CSS instead of continuing to grow the inline server-served prototype

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

```powershell
.\AxiomRemoteViewportServer.exe --host 127.0.0.1 --port 8080 --width 1280 --height 720 --jpeg-quality 75
```

Then open [http://127.0.0.1:8080](http://127.0.0.1:8080) in a browser on the same machine.

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
- remote-style command submission can reuse the same session authority path as the local adapter
- a browser can connect to the headless authoritative session over a real network boundary and drive the existing viewport camera commands
- the browser client can now receive pushed binary viewport frames over WebSocket instead of re-requesting image files every frame

This prototype does not yet provide:

- the low-latency smoothness expected from the real remote editor target
- WebRTC or H.264 streaming
- a dedicated `EditorFrontend` application workspace at the repository root
- a full browser editor shell beyond the viewport-focused prototype
- a production-ready remote viewer/editor client
- a replacement for the current local windowed editor executable

## Next Priority

The next remote-viewport milestone should prioritize:

- WebRTC session transport
- H.264 viewport encoding
- migration of the browser UI from the current inline server-served prototype into a root-level `EditorFrontend` folder

The current localhost:8080 page should be treated as a temporary bring-up client. It remains useful for validating the engine/session boundary, but it should not become the final home of the editor UI.
