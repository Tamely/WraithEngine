# Headless Axiom Session Prototype

`AxiomHeadless` is a prototype executable that runs the Axiom editor session without a GLFW window, accepts newline-delimited JSON commands on `stdin`, and writes captured PNG frames plus NDJSON status/events.

`AxiomRemoteViewportDevClient` is a companion dev executable that exercises the same authoritative session through the new transport seam and writes the frames it receives as a transport subscriber.

`AxiomRemoteViewportServer` is now the primary remote viewport prototype backend. It runs the authoritative headless session and exposes the HTTP/WebRTC endpoints used by the browser editor.

The current slice includes a macOS-first H.264 path that is wired into the native WebRTC sender. The browser viewport consumes the WebRTC video track as the only remote viewport media path.

## Current Status

- Status: working prototype
- Verified on Windows as of 2026-05-05
- Builds on macOS as of 2026-05-07
- Runtime validation on macOS requires a Vulkan/MoltenVK-capable environment with Metal available
- This subphase is complete for the runtime-side seam restoration work
- `AxiomHeadless` is a command-driven authoritative runtime, not a full editor client
- viewport frames are emitted through the engine/session frame-output seam and then written to PNG by the prototype executable
- `ISessionTransport` now exists as the engine-facing remote boundary
- `AxiomSessionEndpoint` is the first in-process transport implementation
- `AxiomRemoteViewportDevClient` is a dev harness for transport-delivered frames/events, not a browser client
- `AxiomRemoteViewportServer` is the current browser-facing backend for remote viewport work
- the current server/browser slice has moved from SSE plus HTTP image polling to WebRTC video plus data channels
- encoded video packet delivery now exists as an additive transport seam beside raw viewport frame delivery
- `IVideoEncoder` now exists as the engine-owned video encode boundary
- a macOS `VideoToolbox` H.264 encoder path now exists for headless remote-viewport bring-up
- `AxiomRemoteViewportServer` now treats WebRTC as the only supported remote viewport media path
- the old frame-ownership splitter and round-robin render-target path have been removed
- headless remote rendering now uses one authoritative `EditorSession`, one shared GPU resource world, and one render view per connected remote client
- the authoritative session scene is now renderer-agnostic and no longer stores renderer-owned mesh submissions as its source of truth
- startup scene loading now populates logical scene mesh instances from the current `basicmesh.glb` asset mapping instead of calling directly into the renderer singleton
- local windowed rendering and headless rendering both rebuild render submissions through a shared `EditorSceneRendererAdapter`
- the headless host now performs one render pass per active remote render view during a single engine tick
- remote `set_view_mode` is now per-client state, not a global remote-server toggle
- the main-loop throttle in the headless remote viewport server has been removed so the runtime can tick at full cadence
- the macOS `VideoToolbox` encoder is now tuned for lower latency by avoiding per-frame synchronous completion, tightening bitrate/rate limits, and shortening the keyframe interval
- the native WebRTC sender now prefers the freshest available H.264 packet instead of rewinding to older packets during normal latest-only delivery
- the browser client now pumps camera/input updates on `requestAnimationFrame` and flushes pointer-lock look input immediately instead of batching on a fixed timer
- the current stream no longer has the severe FPS collapse seen in the older prototype, but there is still roughly half a second of residual input latency to investigate later
- a multi-client frame-routing bug was fixed by stamping each offscreen capture with the submitting `SessionUserId` at render time instead of inferring ownership later from mutable active-pass state
- a root-level `EditorFrontend` workspace now hosts the primary browser editor shell using Next.js, React, and Tailwind CSS
- `EditorFrontend` includes the docked editor layout, menu bar, toolbar, outliner, details panel, content browser, and the active WebRTC viewport client in `components/engine/viewport.tsx`
- the old inline localhost:8080 page has been retired; the server now focuses on backend/session and diagnostics endpoints
- sandboxed validation on macOS can hide the availability of VideoToolbox H.264 encoders; authoritative H.264 validation should be done outside the sandbox on a Vulkan/MoltenVK-capable machine

## Remote Viewport Architecture

The current remote viewport stack is organized as:

- one authoritative `EditorSession`
- one shared renderer and GPU resource cache
- one headless render view per connected remote client
- one WebRTC session and one encoder per connected remote client
- one multi-pass headless engine tick that renders all active remote views sequentially

Important implementation notes:

- render-view state is per client and currently includes at least `SessionUserId`, client id, and view mode
- per-client camera state remains authoritative in `EditorSession`
- presence overlays are assembled per rendered user so a viewer sees other participants, not their own marker
- offscreen frame routing is keyed by `ViewportFrame.User`
- the headless bridge now only fills in a frame user when one was not already stamped by the renderer

Retired paths:

- no frame splitter
- no active render-user ownership rotation
- no `/frame`, `/h264`, or `/h264/metadata` server fallback endpoints
- no JPEG fallback publishing in normal browser operation

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
./AxiomRemoteViewportServer --host 127.0.0.1 --port 8080 --width 1280 --height 720
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
- the major FPS bottlenecks in the remote viewport prototype have been removed through server-loop, duplicate-work, encoder, and input-pump latency fixes
- multiple connected browser clients now have distinct render views instead of sharing a split single-frame ownership path

This prototype does not yet provide:

- the low-latency input response expected from the final remote editor target
- a production-ready remote viewer/editor client
- a replacement for the current local windowed editor executable

## Scene Authoring Progress

The authoritative scene-authoring loop has advanced on the `scene-editing` branch:

- `CreateObjectCommand`, `DuplicateObjectCommand`, and `DeleteObjectCommand` are now implemented as validated authoritative commands with matching `ObjectCreatedEvent` and `ObjectDeletedEvent` events
- all scene objects are now backed by a `DataModel`-rooted Instance hierarchy (`Axiom/CoreInstance/SceneInstances.h`); `EditorSession` owns the live tree and keeps `EditorSceneState::Items` synchronized as a derived projection
- `SetSceneState` and `SetSceneItems` rebuild the Instance tree from snapshot data, enabling round-trip rehydration
- deep subtree duplication clones all descendant Instances with fresh unique IDs and display names
- delete removes the entire subtree from the Instance tree, `ObjectDetailsById`, `MeshInstances`, and any active user selections
- 16 focused tests cover creation, duplication, deletion, all rejection cases, uniqueness generation, and snapshot rehydration

### Reparent and Hierarchical Transforms

- `ReparentObjectCommand` / `ObjectReparentedEvent` are now implemented: any object can be dragged onto any other object in the outliner and becomes a logical child of it
- `Instance::SetParent` cycle prevention is complemented by explicit validation that rejects reparenting an object onto itself or any of its descendants
- the outliner drag-and-drop targets are no longer limited to folder items; any scene item (mesh, light, camera, actor, folder) can receive a drop
- transforms are now stored in two layers: `EditorObjectDetails::Transform` holds local-space L/R/S; `WorldTransform` holds computed world-space L/R/S
- `ComputeWorldTransformMatrix` walks the instance parent chain and multiplies each ancestor's local matrix to produce the object's world matrix
- `DecomposeMatrix` extracts position, YXZ Euler angles (matching `BuildTransformMatrix` rotation order), and scale from a world matrix
- `RecomputeSubtreeWorldTransforms` recomputes `WorldTransform` and updates the renderer mesh instance matrix for a moved object and all its descendants in one pass
- reparenting a child object preserves its stored local `Transform` values unchanged; the rendered world position shifts to be relative to the new parent's world transform
- `SetTransformCommand` (gizmo drag output) arrives in world space; the handler inverts the parent's world matrix to derive local-space storage, then propagates new world transforms to any children
- `SerializeObjectDetails` serializes `WorldTransform` to the browser so the properties panel and gizmo always work in world space regardless of hierarchy depth
- the gizmo overlay position, hit-test, and drag-start all read `WorldTransform->Location` so the gizmo tracks the object's actual rendered position
- 4 reparent-specific tests cover the happy path, cycle rejection, unknown-parent rejection, and self-reparent rejection

## Gizmo System

A server-side transform gizmo is now fully implemented on the `scene-editing` branch:

### Rendering (`VulkanGizmoRenderer`)
- a dedicated Vulkan pipeline draws gizmo handles as billboard line-segment quads inserted between mesh rendering and the offscreen capture step
- **Translate mode**: three colored axis arrows (X=red, Y=green, Z=blue)
- **Scale mode**: same arrows with perpendicular cross-caps at the tips to distinguish them visually
- **Rotate mode**: three 24-segment rings, one per axis, drawn in the plane perpendicular to each axis direction
- the hovered handle brightens in all modes; the gizmo is invisible when no object is selected
- handle size is screen-space-constant: arm length is computed each frame so the gizmo appears at a fixed pixel size regardless of camera distance

### Hit Testing and Drag Math (`Headless/GizmoHitTest.h`)
- `HitTestGizmoAxes`: projects each arrow to screen space and tests 2D point-to-segment distance
- `HitTestGizmoRings`: projects 24 ring segments per axis and tests against each, returning the closest axis within threshold
- `BeginGizmoDrag` / `UpdateGizmoDrag`: axis-constrained drag using a camera-facing constraint plane and ray-plane intersection; returns new world position
- `BeginGizmoRotateDrag` / `UpdateGizmoRotateDrag`: projects mouse onto the ring plane, computes angle delta via `atan2`, returns degrees; wraps to `[-π, π]`
- scale drag reuses the translate constraint plane but converts the axis displacement to a scale multiplier relative to the gizmo arm length at drag start

### Protocol
- `gizmo_hover` (unreliable channel): sent on every `mousemove` that is not a camera drag; server updates the highlighted handle each frame
- `gizmo_drag_start` / `gizmo_drag_end` (reliable): bracket the drag; drag start resolves the hit handle and captures object state; drag end commits the final transform
- `gizmo_drag_update` (unreliable): sent every `mousemove` during drag; server applies the current drag math and dispatches `SetTransformCommand`
- `set_gizmo_mode`: switches the per-client gizmo mode (`translate` / `scale` / `rotate`); server updates hit-test and rendering for subsequent frames
- snapshot refresh is suppressed while a gizmo drag is active to prevent server state polls from fighting the in-progress drag position

### Mode Switching
- `GizmoMode` enum (Translate / Scale / Rotate) lives in `RenderScene.h` and is passed through `GizmoOverlayData` to the renderer
- per-client mode is stored in `RemoteClientSession::CurrentGizmoMode` and in `HeadlessSessionLayer` for the render path
- the toolbar Move, Rotate, and Scale buttons are now wired to `setGizmoMode` from `RemoteViewportContext`; active-state styling reflects the current mode
- Q = Translate, E = Scale, R = Rotate keyboard shortcuts fire when the viewport is focused and not in camera look mode
- `gizmoMode` state lives in `RemoteViewportContext` so toolbar and viewport share a single source of truth

### Coordinate Mapping Fix
- mouse pixel coordinates sent to the server account for the `object-contain` CSS letterboxing on the video element: the actual content rectangle is computed from the uniform scale factor and centering offsets before mapping to server pixels, so hit-testing is accurate regardless of window aspect ratio

## Viewport Object Interaction

- dragging a mesh asset from the content browser into the remote viewport now creates a new mesh object directly at the resolved drop point; the server prefers mesh-surface hit, then ground-plane intersection, then a fixed point in front of the camera
- visible light objects are rendered as camera-facing `lightbulb.svg` billboards tinted from the light color
- remote viewport click selection now considers those light billboards in addition to mesh hits
- selection remains gizmo-first; if no gizmo handle is hit, the server compares the nearest mesh hit and nearest light-billboard hit and selects whichever is closer in world space
- billboard selection targets the underlying light object id; hidden lights do not expose selectable billboards

## Collaboration v1

Object locking, selection/lock visibility, presence indicators, and heartbeat-driven idle detection are now fully implemented.

### Object Locking

- `EditorObjectLockState` (`Unlocked` / `Locked`) lives in `Axiom/Session/SessionTypes.h` to avoid a circular header dependency between `EditorSession.h` and `EditorEvent.h`
- `EditorObjectCollaborationState` carries `LockState` and `LockOwner` per object inside `EditorSessionState`
- `ObjectLockChangedEvent` is added to the `EditorEventPayload` variant and serialized as `{ "type": "object_lock_changed", "objectId": ..., "lockState": "locked"|"unlocked", "lockOwnerUserId": n|null }`
- `EditorSession` exposes three public methods: `AcquireLock(ObjectId, UserId)`, `ReleaseLock(ObjectId, UserId)`, and `ReleaseAllLocksForUser(UserId)`; each publishes an `ObjectLockChangedEvent` broadcast to all clients
- `RemoteViewportServer` calls `AcquireLock` at gizmo drag start and `ReleaseLock` at gizmo drag end, so the dragging user holds an exclusive transform lock for the duration of the interaction
- `ValidateCommand` in `EditorSession` rejects `SetTransform`, `Rename`, `SetObjectVisibility`, `Delete`, and `Reparent` commands on an object locked by a different user; the command is dropped with a `CommandRejected` event

### Selection and Lock Visibility in the Outliner

- the outliner renders a small avatar chip (colored initial letter) for each other participant whose `selectionObjectId` matches the row's object ID, using `participant.presentationColor` as the background
- if an object is locked, a lock icon appears in the locking user's presentation color; hovering the icon shows the owner's display name as a tooltip
- `lockedObjects: Record<string, number>` is maintained in `RemoteViewportContext` and updated by `object_lock_changed` events; both the lock icon and the chip data are read from context with no prop drilling

### Presence Roster

- `EditorFrontend/components/engine/presence-roster.tsx` renders a horizontal strip of avatar chips in the toolbar, one per non-disconnected participant
- each chip shows the user's initial letter on a `presentationColor` background with a small status dot: green = connected, yellow = away, grey = disconnected
- the roster reads `participants` from `RemoteViewportContext` and is mounted in the toolbar between the spacer and the play controls

### Heartbeat Protocol

- the browser sends `{ "type": "heartbeat" }` over the reliable WebRTC data channel every 4 seconds while connected
- the server resets `Client->LastActivity` on each heartbeat; if the client was Away it is promoted back to Connected and the presence update is broadcast
- a dedicated `PresenceLoop` thread wakes every 2 seconds and checks elapsed time since `LastActivity` for every connected client:
  - elapsed ≥ 10 s and Connected → `SetPresenceState(Away)`, broadcast
  - elapsed ≥ 30 s and Away → `SetPresenceState(Disconnected)`, `ReleaseAllLocksForUser`, broadcast
- the two-threshold design handles hard tab closes: JavaScript cleanup never fires, so the heartbeat simply stops, the client goes Away at 10 s and Disconnected at 30 s, and all locks are released at the Disconnected transition

## Next Priority

- deeper WebRTC sender/playout latency tuning for the remote viewport stream
