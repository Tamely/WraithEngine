# Headless Axiom Session Prototype

`AxiomHeadless` is a prototype executable that runs the Axiom editor session without a GLFW window, accepts newline-delimited JSON commands on `stdin`, and writes captured PNG frames plus NDJSON status/events.

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
- `set_look_active`
- `update_viewport_camera`
- `render_frame`
- `quit`

Example session:

```json
{"type":"load_startup_scene"}
{"type":"set_look_active","isLooking":true,"cursorPosition":[100.0,120.0]}
{"type":"update_viewport_camera","worldMovement":[0.0,0.0,-0.25],"cursorPosition":[108.0,116.0]}
{"type":"render_frame"}
{"type":"quit"}
```

## Frame Files

Each `render_frame` command writes one numbered PNG in `--output-dir`:

- `frame_000001.png`
- `frame_000002.png`

The corresponding `frame` message includes the file path and output dimensions.

## Ordering Rules

- `render_frame` before `load_startup_scene` returns an `error`
- repeating `load_startup_scene` returns an `error`
- malformed JSON or unsupported command types return an `error`
- failing to create the output directory or write a PNG returns an `error`
