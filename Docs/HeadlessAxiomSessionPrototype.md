# Headless Axiom Session Prototype

`AxiomHeadless` is a prototype executable that runs the Axiom editor session without a GLFW window, accepts newline-delimited JSON commands on `stdin`, and writes captured PNG frames plus NDJSON status/events.

`AxiomRemoteViewportDevClient` is a companion dev executable that exercises the same authoritative session through the new transport seam and writes the frames it receives as a transport subscriber.

`AxiomRemoteViewportServer` is now the primary remote viewport prototype. It runs the authoritative headless session, serves a browser client over HTTP, streams JSON session messages over server-sent events, and exposes the latest captured PNG frame at `/frame`.

## Current Status

- Status: working prototype
- Verified on Windows as of 2026-05-04
- This subphase is complete for the runtime-side seam restoration work
- `AxiomHeadless` is a command-driven authoritative runtime, not a full editor client
- viewport frames are emitted through the engine/session frame-output seam and then written to PNG by the prototype executable
- `ISessionTransport` now exists as the engine-facing remote boundary
- `AxiomSessionEndpoint` is the first in-process transport implementation
- `AxiomRemoteViewportDevClient` is a dev harness for transport-delivered frames/events, not a browser client
- `AxiomRemoteViewportServer` is the current browser-facing demo path for remote viewport work

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
.\AxiomRemoteViewportServer.exe --host 127.0.0.1 --port 8080 --width 1280 --height 720
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

This prototype does not yet provide:

- WebRTC or H.264 streaming
- a full browser editor shell beyond the viewport-focused prototype
- a production-ready remote viewer/editor client
- a replacement for the current local windowed editor executable
