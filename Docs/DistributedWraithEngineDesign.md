# Distributed Wraith Engine Design

## Document Status
- Status: Draft
- Date: 2026-05-08
- Audience: Engine, tools, networking, web, and infrastructure contributors
- Intended outcome: Establish the target architecture for evolving WraithEngine into a distributed game engine and browser-based collaborative editor

## Implementation Progress
- `event-system` branch now contains the first local authoritative editor-session slice
- Added engine-owned `EditorSession`, `EditorCommand`, `EditorEvent`, `SessionId`, and `SessionUserId` foundations in `Axiom`
- The native editor now translates GLFW input into commands and renders from session-owned camera/scene state instead of mutating camera state directly in the layer
- Added deterministic in-process command draining, authoritative event publication, and focused tests for camera/look state transitions and command rejection
- Restored the `GLFW split` and application/runtime seams after the `headless` merge so local windowed input remains an adapter rather than the editor authority boundary
- `AxiomHeadless` now boots successfully in headless Vulkan mode, renders offscreen, and publishes viewport frames back through `AxiomSessionEndpoint`
- Added engine-facing `ISessionTransport` and made `AxiomSessionEndpoint` the first in-process transport implementation
- Added `AxiomRemoteViewportDevClient` as a transport-subscriber harness that receives authoritative events and writes client-received frames to disk
- Added `AxiomRemoteViewportServer` as the first real browser-facing remote viewport prototype, evolving from HTTP image polling to WebSocket/JPEG bring-up and now to a native macOS WebRTC plus H.264 browser path
- Added engine-owned encoded-video packet types plus `IVideoEncoder` and extended `AxiomSessionEndpoint` so encoded packets can flow beside raw viewport frames
- Added a macOS-first `VideoToolbox` H.264 encoder path for headless remote-viewport bring-up
- `AxiomRemoteViewportServer` now treats WebRTC as the only supported remote viewport media path
- Removed the largest remote-viewport performance bottlenecks by unthrottling the headless server loop and tuning the encoder/input path for latency
- The remote viewport now runs at acceptable frame rate, but still has noticeable residual input latency that likely requires deeper WebRTC sender/playout tuning
- A root-level `EditorFrontend` workspace now serves as the longer-lived browser editor shell using Next.js, React, and Tailwind CSS
- `EditorFrontend` contains a docked editor UI with a menu bar, toolbar, outliner, details panel, content browser, and the active WebRTC viewport client
- `EditorSession` scene authority has been decoupled from renderer-owned submissions and now stores renderer-agnostic logical scene mesh instances
- local and headless rendering now rebuild renderer submissions through a shared adapter instead of treating renderer-owned objects as session state
- the headless remote viewport path now uses per-client render views rendered sequentially in one engine tick instead of a single-frame ownership splitter
- remote view mode is now per client
- a delayed-readback frame attribution bug in multi-pass headless rendering was fixed by stamping each offscreen capture with the submitting `SessionUserId` at submission time
- The next browser-facing step after the migration is turning the browser shell plus authoritative session into a real single-user scene editor, not more work on a server-hosted prototype page
- Collaboration should continue to follow that same authoritative command/event path after the single-user authoring loop is stable, rather than leading the roadmap ahead of core editor behavior
- `scene-editing` branch introduces the first authoritative object-lifecycle commands: `CreateObjectCommand`, `DuplicateObjectCommand`, and `DeleteObjectCommand`, with matching `ObjectCreatedEvent` and `ObjectDeletedEvent` authoritative events
- All scene objects are now backed by an Instance-class hierarchy rooted at a `DataModel` node, mimicking the Roblox object model; `EditorSession` owns the live `DataModel` tree and keeps `EditorSceneState::Items` synchronized as a derived projection
- Concrete scene Instance subclasses introduced: `SceneFolder`, `SceneMeshObject`, `SceneLight`, `SceneCamera`, and `SceneActor` under `Axiom/CoreInstance/SceneInstances.h`
- `SetSceneState` and `SetSceneItems` now rebuild the Instance tree from snapshot data, enabling round-trip snapshot rehydration
- Deep subtree duplication is supported: duplicating a folder clones all descendant Instances and their `ObjectDetails` entries with fresh unique IDs and display names
- Delete removes the entire Instance subtree, strips all descendant entries from `ObjectDetailsById` and `MeshInstances`, and clears any selections pointing at deleted objects
- New object creation always parents to the "world" `SceneFolder` (direct child of `DataModel`); valid built-in templates are `Folder`, `Mesh`, `Light`, `Camera`, and `Actor`
- Added 16 focused tests in `Tests/SceneLifecycleTests.cpp` covering create, duplicate, delete, all rejection cases, uniqueness generation, and snapshot rehydration
- The object lifecycle commands are now fully wired through the browser editor shell: `HeadlessCommandType` and `ParseRemoteViewportCommand` were extended for `create_object`, `duplicate_object`, and `delete_object`; `object_created` and `object_deleted` events are now serialized and broadcast to all connected clients
- The outliner gained an Add dropdown (Folder, Mesh, Light, Camera, Actor templates), a Delete toolbar button active when an object is selected, and right-click context menus on every scene item with Duplicate and Delete entries; all three operations dispatch through the same authoritative command/event path
- The `RemoteViewportCommand` TypeScript union was extended for the three new command types; the event dispatch block in `viewport.tsx` handles `object_created` and `object_deleted` by triggering a session snapshot refresh so the outliner updates automatically
- A server-side transform gizmo is now implemented end-to-end: the engine renders colored axis handles as a Vulkan overlay pass on top of the selected object in the captured frame, and the browser forwards mouse input so the server can hit-test and drag those handles to drive `SetTransformCommand`
- `GizmoMode` (Translate / Scale / Rotate) is now a first-class per-client concept; the toolbar Move, Rotate, and Scale buttons switch modes with active-state feedback, and Q/E/R keyboard shortcuts work while the viewport is focused
- Translate mode constrains drag to the clicked axis via ray-plane intersection; Scale mode applies a proportional delta to the clicked axis scale component; Rotate mode projects the mouse onto each ring's plane and converts the angle delta to degrees
- `VulkanGizmoRenderer` draws mode-appropriate handles: arrows for translate, arrows with perpendicular cross-caps for scale, and 24-segment screen-space rings for rotate; the hovered handle brightens in all modes
- Gizmo mouse coordinates are forwarded using the correct `object-contain` content rect mapping so hit-testing is accurate regardless of the viewport aspect ratio or window size
- `gizmoMode` state lives in `RemoteViewportContext` so the toolbar and viewport share a single source of truth without prop drilling
- Collaboration v1 is now implemented: object locking, selection/lock visibility, presence roster, heartbeat-driven idle detection, and two-threshold disconnect (Away at 10 s, Disconnected at 30 s with lock release)
- `EditorObjectLockState` (`Unlocked` / `Locked`) and `EditorObjectCollaborationState` live in `EditorSessionState`; `AcquireLock`, `ReleaseLock`, and `ReleaseAllLocksForUser` are public `EditorSession` methods
- `ValidateCommand` rejects any mutating command (`SetTransform`, `Rename`, `SetObjectVisibility`, `Delete`, `Reparent`) on an object locked by a different user
- gizmo drag start/end bracket lock acquisition and release in `RemoteViewportServer`; hard disconnect and presence timeout release all locks for the departing user
- `ObjectLockChangedEvent` is serialized as `object_lock_changed` and consumed by the browser to keep a `lockedObjects` map in `RemoteViewportContext`
- the outliner renders per-row avatar chips for selecting users and a lock icon in the locking user's color; the `PresenceRoster` component shows participant chips with green/yellow/grey status dots in the toolbar
- heartbeat: browser sends `{ "type": "heartbeat" }` on the reliable data channel every 4 s; `PresenceLoop` background thread transitions Connected → Away at 10 s and Away → Disconnected at 30 s of inactivity
- Phase 5 (Reflection and Asset Evolution) is now implemented: `AssetId` stable identity type, `IAssetSource` / `LocalAssetSource` VFS abstraction, `ListAssets` / `GetSchema` / `SetProperty` / `SaveScene` commands, `SceneFile` JSON serializer/parser, and content browser + details panel wired to live server data
- `AssetId` mirrors `SessionUserId` with a stable hash-derived `uint64_t` value; `LocalAssetSource` scans a root directory for `.glb`, `.gltf`, `.png`, `.jpg`, `.jpeg` files and derives IDs from hashed relative paths
- `IAssetSource` interface exposes `Resolve(AssetId)` → `optional<path>` and `List()` → `vector<AssetDescriptor>`; engine-internal paths use `ResolveRelative` as a typed wrapper over the `AXIOM_CONTENT_DIR` macro (retained for shaders)
- `ListAssets` command returns `{"type":"asset_list","assets":[{id,name,kind,path}...]}`; the content browser triggers it on connection and renders real assets in grid/list views with mesh/texture filter tabs and a Refresh button
- `GetSchema` command returns `{"type":"object_schema","objectId":"...","className":"...","properties":[{name,type,readOnly}...]}`; the details panel fetches schema on selection change and shows a `className` badge in the panel header
- `SetProperty` command dispatches to `RenameObjectCommand`, `SetObjectVisibilityCommand`, or `SetTransformCommand` based on property name; vec3 fields (location, rotationDegrees, scale) read the current transform from `ObjectDetailsById` and patch only the changed component
- `SceneFile` (`Axiom/Assets/SceneFile.h/.cpp`) provides `SaveSceneToFile` / `LoadSceneFromFile`; serialization uses a manual `ostringstream` JSON emitter in flat-node format with `parentId` links; deserialization uses a purpose-built recursive descent parser (no external JSON library)
- `LoadStartupScene` now checks for `Content/scene.json` first and falls back to the hardcoded default scene; scene state persists across server restarts automatically
- `SaveScene` command writes `Content/scene.json` and replies with `{"type":"scene_saved"}` or `{"type":"scene_save_failed"}`; the toolbar Save button animates to a green checkmark for 2.5 s on success or a red X on failure

## 1. Executive Summary
WraithEngine will evolve from a single-process native editor into a distributed platform with one shared C++ engine runtime that supports two execution styles:

- `Local packaged runtime`: a native executable with cooked assets that runs on the user's machine
- `Hosted authoritative runtime`: a headless or remotely managed engine session that runs on a server and streams a viewport to a browser client

The first major milestone is a `remote browser editor`. The browser will own most editor UI using React, Next.js, and Tailwind CSS, while a native engine session remains authoritative for rendering, world state, asset loading, and validation of edits. The viewport will be rendered server-side and streamed to the browser via WebRTC using H.264 first.

The current repository is still a native Vulkan/GLFW/ImGui engine with a simple `Application -> Renderer -> RenderScene` flow, file-based mesh loading, and a native editor executable. That is a good base for the next phase because the rendering and runtime already exist, but the codebase needs new seams for headless execution, transport, collaboration state, asset reflection, scripting isolation, and deployment.

This document describes the target architecture, public concepts, service boundaries, trust model, rollout order, and acceptance criteria for that transition.

## 2. Goals and Non-Goals

### 2.1 Goals
- Support a browser-based remote editor backed by a native authoritative engine session
- Preserve one shared core runtime for both local packaged games and hosted sessions
- Enable low-latency streamed viewport interaction in the browser
- Add collaboration with presence, selection visibility, and locking as the first version
- Add C# scripting with a sandbox-first hosted trust model
- Build a reflection-based asset and property system suitable for browser inspection, serialization, scripting, and replication
- Keep the architecture suitable for future hosted multiplayer games
- Preserve the current native editor path as a bootstrap and debugging client during the transition

### 2.2 Non-Goals for the First Milestone
- Full Google-Docs-style concurrent code co-editing
- Full CRDT-based scene editing
- Fully general third-party server plugin execution in hosted sessions
- Perfect parity between local native editor UI and browser editor UI
- Full cloud orchestration, billing, and production SaaS features
- Finalized gameplay networking stack for every genre
- A finished asset cooking system for every asset class on day one

## 3. Product Vision
The long-term product is a game engine where creators can:

- Build games from a browser without requiring a powerful local machine
- Stream high-end rendered content to low-power devices such as Chromebooks
- Package projects into native executables for local distribution
- Host game sessions on servers using the same core simulation/runtime stack
- Collaborate on worlds in real time with visible teammate presence in the editor
- Script gameplay and tools in C# with explicit safety boundaries

This implies the engine is not just a renderer or editor. It becomes a platform composed of:

- a native engine runtime
- a collaborative editing layer
- a web application
- a session/control backend
- a packaging and deployment pipeline

## 4. Current State of the Repository
At the time of writing, the repository contains:

- `Axiom`: the current runtime library name and the future remote/distributed subsystem brand
- `Editor`: a native editor executable
- `Tests`: basic test scaffolding
- a Vulkan renderer backend
- GLFW window creation and event polling
- ImGui-based native tooling/debug UI
- mesh scene loading from `.glb`

The active architecture is roughly:

1. `Application` owns a GLFW window and a renderer.
2. `Layer`s update every frame and can render from authoritative state in a separate pass.
3. A local `EditorSession` owns viewport camera/look state and drains queued commands deterministically each frame.
4. `RenderCommand` writes authoritative frame data into a frame-local `RenderScene`.
5. `Renderer` passes that scene into a Vulkan backend.
6. The backend renders to a window-presented swapchain image.

This is enough to establish:

- there is already a separation between scene submission and rendering
- there is now an initial engine-owned command/event authority seam for editor viewport state
- the renderer can evolve into windowed and headless targets
- the current authoritative domain is still narrow and local to viewport/input state rather than full world editing
- there is not yet a reflection, network, collaboration, headless runtime, or scripting host layer

## 5. Architectural Principles
- One core runtime, multiple adapters
- Server-authoritative state for hosted editing and hosted gameplay
- Browser owns editor chrome; engine owns authoritative simulation and pixels
- Headless support is a first-class runtime mode, not a test-only hack
- Reflection metadata is engine-owned and language-agnostic
- Trust is explicit and tiered
- Collaboration starts simple with presence and locking
- Runtime/session protocols use stable object identity, not file paths
- Hosted and local packaging reuse as much code as possible

## 6. High-Level System Overview

### 6.1 Main Runtime Roles
The distributed platform is made of three primary roles:

1. `Engine Runtime`
   - Native C++ runtime
   - Runs simulation, asset loading, rendering, scripting host, and authoritative editor/game state
   - Can run locally or in hosted mode

2. `Editor Control Service`
   - Owns session lifecycle and orchestration
   - Authenticates users
   - Opens projects
   - Tracks participants and locks
   - Bridges browser connections to engine sessions

3. `Web Client`
   - React/Next.js/Tailwind app
   - Owns all browser editor UI outside the streamed viewport
   - Connects to control APIs and realtime session transport

Current implementation direction:

- use the existing root-level `EditorFrontend` folder as the home for the browser editor shell
- keep `EditorFrontend/components/engine/viewport.tsx` as the canonical browser WebRTC client
- keep `AxiomRemoteViewportServer` focused on session, signaling, command, and diagnostics endpoints

### 6.2 Conceptual Topology
```text
Browser Client
  ├─ HTTP/HTTPS -> Control Service
  └─ WebRTC -> Engine Session

Control Service
  ├─ Session orchestration
  ├─ Project open/save workflows
  ├─ User presence and lock coordination
  └─ Engine session launch/lookup

Engine Session
  ├─ Authoritative editor state
  ├─ Rendering
  ├─ Frame capture + encode
  ├─ Asset system
  ├─ Script host
  └─ Command validation + event broadcast
```

## 7. Execution Modes

### 7.1 Runtime Modes
The engine runtime should support at least these modes:

- `LocalWindowedEditor`
  - Native desktop editor
  - Window + Vulkan swapchain
  - ImGui can remain available for debugging and transition tooling

- `HeadlessEditorSession`
  - Hosted editor session
  - No OS-visible desktop window requirement
  - Offscreen rendering target
  - Frame capture and video encode enabled

- `LocalPackagedGame`
  - Native executable for end-user distribution
  - Can use local windowed presentation

- `HostedGameSession`
  - Server-hosted runtime session for browser-played or networked games
  - Can reuse much of the headless infrastructure

### 7.2 Why This Split Matters
Local and hosted products must differ mainly by adapters and configuration:

- presentation adapter
- input adapter
- launch/bootstrap config
- trust profile
- deployment topology

They should not differ by simulation model, asset identity rules, or gameplay API shape.

## 8. Browser Editor Model

### 8.1 Responsibilities of the Web Client
The browser application owns:

- login/session selection UI
- project browser
- outliner / scene tree
- property inspector
- content browser
- collaboration roster
- lock and presence indicators
- build/deploy controls
- console/log UI
- browser-resident editor tools that do not require native rendering

### 8.2 Responsibilities of the Engine Session
The engine session owns:

- authoritative scene/world state
- transform and property mutation validation
- rendering
- asset loading and cooking-facing metadata
- script execution
- authoritative session presence state
- lock enforcement
- viewport camera state per connected user

### 8.3 Viewport Interaction Model
The browser viewport is a streamed surface backed by the native engine session. The client sends:

- pointer input
- keyboard input
- camera movement intents
- gizmo interaction intents
- selection commands

The server returns:

- video frames
- authoritative state changes
- selection updates
- presence updates
- lock updates
- command acknowledgements/failures

The browser should never be considered the authority for world state.

## 9. Realtime Transport

### 9.1 Chosen v1 Transport
WebRTC is the first transport target because it provides:

- low-latency browser-native video
- built-in NAT traversal support patterns
- reliable and unreliable data channels
- good fit for streamed viewport plus bidirectional input

### 9.2 Channel Plan
- `Video track`
  - H.264 in v1
  - primary use: streamed viewport

- `Audio track`
  - optional later
  - not required for the first editor milestone

- `Reliable data channel`
  - session RPC
  - selection and property edits
  - asset operations
  - lock state
  - presence updates
  - save/build/deploy requests

- `Unreliable data channel`
  - high-frequency camera transform deltas
  - mouse-look deltas
  - temporary cursor hover information if needed

### 9.3 Transport Abstraction
The engine should expose an `ISessionTransport` interface that does not hardcode WebRTC into higher-level editor logic. The first implementation is WebRTC, but the engine-facing APIs should speak in terms of:

- connect/disconnect
- send event
- receive command
- subscribe to user/session lifecycle
- publish encoded frame stream

## 10. Rendering Architecture Evolution

### 10.1 Current Constraint
The current Vulkan backend presents directly to a swapchain tied to a GLFW window. That is appropriate for the native editor, but it is not enough for hosted sessions.

### 10.2 Required Rendering Seams
Rendering must be split into:

- `Scene rendering`
- `Render target/output abstraction`
- `Frame capture`
- `Video encoding`
- `Optional presentation`

### 10.3 Render Surface Abstraction
Introduce an `IRenderSurface` concept so the renderer can target:

- `WindowRenderSurface`
  - swapchain-backed
  - used by local desktop runtime

- `OffscreenRenderSurface`
  - image-backed
  - used by hosted headless sessions

This abstraction should decide:

- extent
- presentation capability
- resize behavior
- image acquisition/release semantics

### 10.4 Frame Capture
Introduce `IFrameCapture`:

- receives completed render targets
- performs readback or zero-copy transfer where supported
- outputs a frame representation suitable for encoding

### 10.5 Video Encoding
Introduce `IVideoEncoder`:

- consumes captured frames
- produces transportable encoded packets/frames
- starts with H.264
- leaves room for future AV1 or HEVC support

### 10.6 Local Native Path
The local native path should still work even after these abstractions exist:

- render to window-backed surface
- optionally expose captured frames for recording or diagnostics
- keep ImGui debug UI available during transition

## 11. Editor Session Authority Model

### 11.1 Core Rule
Hosted editing is server-authoritative. Every change goes through:

`client intent -> validation -> authoritative mutation -> event broadcast`

### 11.2 EditorSessionState
Each live editing session owns an `EditorSessionState` that includes:

- project identity and open project metadata
- loaded world/scene graph
- entity/component/object registry
- selected objects per user
- active gizmo operations
- viewport camera state per user
- presence state per user
- object locks
- asset locks
- session activity metadata

Current implementation note:

- the current slice covers per-user viewport camera state, look/cursor-capture state, last cursor position bookkeeping, presence state, startup-scene logical mesh instances, selection state, and object transform authority for the startup scene
- renderer-owned `RenderMeshSubmission` objects are no longer authoritative session state; they are now rebuilt from logical session scene data through an adapter at render time
- all scene objects are now backed by an Instance hierarchy rooted at `DataModel`; `EditorSession` owns a `std::unique_ptr<DataModel>` and keeps `EditorSceneState::Items` synchronized as a projection of the live tree
- `SetSceneState` and `SetSceneItems` rebuild the Instance tree from the provided snapshot, enabling rehydration and round-trip restore
- entity/component/object registries, locks, presence, and asset editing state remain future work

### 11.3 Why This Matters
The current local editor behavior is mostly frame-local and immediate. That is fine for one user, but collaboration requires:

- serializable operations
- deterministic validation
- user identity on edits
- event rebroadcast
- conflict handling

## 12. Collaboration Model

### 12.1 v1 Scope
Collaboration starts with:

- multi-user presence
- visible camera/avatar/name in the scene
- visible selection/focus state
- object locking
- asset/file locking
- serialized authoritative mutations

### 12.2 Out of Scope for v1
- CRDT-based scene graphs
- free-for-all concurrent mutation of the same object
- Google-Docs-style simultaneous collaborative code editing

### 12.3 Locking Rules
Recommended v1 rules:

- transient selection does not automatically lock
- starting a destructive or write operation can acquire a lock
- transform manipulations lock the target object(s) for the duration of the interaction
- asset metadata edits lock the asset
- lock ownership is visible to all users
- admin/host may forcibly release stale locks

### 12.4 Presence Visualization
Each participant should have:

- user ID
- display name
- session role
- camera transform
- current tool
- current selection
- last activity time

Remote users should appear as lightweight editor-presence actors rather than gameplay entities.

## 13. Command and Event Model

### 13.1 Commands
Commands represent requested mutations or user intentions. Initial editor command set:

- `CreateEntity`
- `DeleteEntity`
- `RenameObject`
- `ReparentObject`
- `SetTransform`
- `AddComponent`
- `RemoveComponent`
- `EditProperty`
- `ImportAsset`
- `AcquireLock`
- `ReleaseLock`
- `UpdateViewportCamera`
- `SetSelection`

Current implementation note:

- `UpdateViewportCamera` is implemented as the first authoritative viewport command
- `SetLookActive` is implemented locally as an editor-session command to control cursor-capture / mouselook authority
- `CreateObject` (with `TemplateId`), `DuplicateObject`, and `DeleteObject` are now implemented as the first authoritative object-lifecycle commands; valid built-in templates are `Folder`, `Mesh`, `Light`, `Camera`, and `Actor`
- `RenameObject`, `SetObjectVisibility`, `SelectObject`, and `SetTransform` are implemented as the core per-object mutation commands

### 13.2 Events
Events represent authoritative outcomes or state broadcasts. Initial event set:

- `EntityCreated`
- `EntityDeleted`
- `ObjectRenamed`
- `ObjectReparented`
- `TransformChanged`
- `ComponentAdded`
- `ComponentRemoved`
- `PropertyEdited`
- `AssetImported`
- `LockGranted`
- `LockDenied`
- `LockReleased`
- `SelectionChanged`
- `PresenceUpdated`
- `UserJoined`
- `UserLeft`
- `CommandRejected`

Current implementation note:

- `ViewportCameraUpdated`, `LookStateChanged`, `CommandRejected`, `CommandAcknowledged` are implemented as the first authoritative session events
- `SelectionChanged`, `ObjectRenamed`, `ObjectVisibilityChanged`, `ObjectTransformUpdated` are implemented as per-object mutation events
- `ObjectCreated` and `ObjectDeleted` are now implemented as the first authoritative object-lifecycle events; `ObjectCreated` carries the new object's stable ID and display name

### 13.3 Validation Rules
Commands should be validated against:

- user permissions
- session/project state
- object existence
- asset existence
- lock ownership
- schema and property type compatibility
- trust/sandbox rules for script-related actions

## 14. Identity Model
Stable identity is mandatory for runtime protocols, collaboration, and persistence.

### 14.1 Required Identity Types
- `SessionId`
- `SessionUserId`
- `ProjectId`
- `AssetId`
- `ObjectId`
- `EntityId`
- `ComponentId`

### 14.2 Identity Rules
- IDs must be stable across save/load operations
- hosted session protocols must refer to objects by stable IDs, not transient pointers or file paths
- IDs must survive cooking where conceptually appropriate
- path changes should not imply asset identity changes

## 15. Reflection and Property System

### 15.1 Purpose
The property system is the engine's central metadata layer for:

- browser property editing
- serialization
- undo/redo
- diff/merge hooks
- scripting bindings
- replication filtering
- asset inspection

### 15.2 Core Property Kinds
Initial property kinds:

- `BoolProperty`
- `IntProperty`
- `FloatProperty`
- `StringProperty`
- `EnumProperty`
- `StructProperty`
- `ObjectRefProperty`
- `ArrayProperty`
- `MapProperty`

### 15.3 Core Metadata Types
- `PropertyType`
- `PropertyDescriptor`
- `ObjectSchema`
- `AssetTypeDescriptor`

### 15.4 Reflection Requirements
Every asset/object/component type exposed to editor or runtime tooling should provide:

- stable schema name
- property list
- property type information
- default values when relevant
- editability flags
- serialization rules
- script visibility flags
- replication flags where relevant

### 15.5 Language Neutrality
Reflection must be engine-owned. C# should consume reflected metadata through the engine API rather than being the source of truth for engine schema.

## 16. Asset System Design

### 16.1 Hybrid Asset Strategy
Asset workflows should separate:

- `Source asset`
  - imported or author-authored source data
  - examples: models, textures, scripts, authored metadata files

- `Asset metadata`
  - stable ID
  - import settings
  - type info
  - dependency graph
  - reflected property graph

- `Cooked asset`
  - runtime-optimized binary data
  - fast load path
  - deployment-friendly

### 16.2 Why Hybrid Instead of Fully Binary Authoring
A hybrid model keeps:

- runtime performance and compact cooked content
- better source control ergonomics for metadata
- easier tooling and migration flows
- cleaner import/cook boundaries

### 16.3 Asset Pipeline Stages
1. Import source content
2. Assign or resolve stable `AssetId`
3. Extract metadata and reflected properties
4. Build dependency graph
5. Save editable project-side metadata
6. Cook optimized runtime payloads
7. Publish manifest for local or hosted runtime

### 16.4 Asset Manifests
The system should define:

- project asset manifest
- cooked asset manifest
- dependency/cook version information
- asset type registry

## 17. Serialization and Persistence
Serialization must support:

- project save/load
- session restore
- undo/redo
- command replay where useful
- asset metadata persistence
- migration across engine versions

Recommended persistence principles:

- object references serialize by stable IDs
- property serialization is schema-aware
- metadata formats should remain human-toolable where practical
- cooked runtime payloads can remain binary and optimized

## 18. Undo/Redo
Undo/redo should be based on the same command/event model as collaborative editing where practical.

Recommended design:

- editor commands produce authoritative mutations
- undo records reference stable object identity and previous values
- multiplayer editing only allows undo of actions owned by that user when safe
- lock ownership and object existence must be revalidated on undo/redo

## 19. Scripting Architecture

### 19.1 Language Choice
C# is the primary scripting language for gameplay and future editor tooling.

### 19.2 Script System Components
- `Engine API Assembly`
  - stable engine-exposed API
  - entities, components, transforms, asset references, gameplay events, selected networking hooks

- `Project Script Assembly`
  - user-authored code
  - compiled against the engine API surface

- `Host Bridge`
  - marshaling boundary between native C++ runtime and managed execution

- `Script Host`
  - loads assemblies
  - manages lifecycle
  - enforces sandbox policy

### 19.3 Script Lifecycle
The script host should support:

- assembly load
- reload
- instance creation
- per-object attachment where appropriate
- start/update/shutdown hooks
- fault capture and isolation

## 20. Trust and Sandbox Model

### 20.1 Tiered Trust
The platform should explicitly support different trust tiers:

- `HostedRestricted`
  - for cloud-hosted or shared sessions
  - tight sandbox
  - only approved engine API surface
  - no arbitrary native plugins

- `LocalTrusted`
  - for local desktop and self-hosted users
  - broader optional capabilities
  - can enable trusted extensions or native interop deliberately

### 20.2 HostedRestricted Constraints
Hosted mode should block or tightly control:

- unrestricted filesystem access
- raw sockets
- arbitrary process spawning
- arbitrary OS API access
- unrestricted native dynamic loading
- unbounded memory/time/resource usage

### 20.3 Isolation Requirements
Hosted sessions should be isolated at least at the process level, and ideally at the container or equivalent environment level once infrastructure matures.

### 20.4 Why Not Max Flexibility Everywhere
Allowing unbounded code execution in hosted sessions would undermine:

- server safety
- multi-tenant reliability
- deterministic deployment
- customer trust

Flexibility should come from trust tiers, not from weakening hosted guarantees.

## 21. Networking Layers

### 21.1 Layer Separation
The system should separate networking into:

- `Control Plane`
  - auth
  - project/session discovery
  - session start/stop
  - save/build/deploy flows
  - orchestration

- `Realtime Editor Plane`
  - viewport stream
  - editor commands/events
  - selection
  - camera movement
  - presence
  - locking

- `Gameplay Networking Plane`
  - game replication
  - gameplay RPC
  - player/session game authority

### 21.2 Important Rule
Editor collaboration protocols are not a substitute for gameplay replication. They can share lower-level transport ideas, but they should remain distinct systems.

## 22. Hosted Gameplay Model
Hosted gameplay should be server-authoritative by default.

Implications:

- core simulation runs on the server
- clients send input/intent
- clients receive replicated outcomes and optionally streamed video
- cheat resistance and consistency are improved
- low-power browser clients become feasible

This does not require that every future game uses pure video streaming only. Some future genres may mix streamed rendering and state replication differently, but the hosted authority default remains server-side.

## 23. Packaging and Deployment

### 23.1 Product Outputs
The build/deployment pipeline should produce:

- `Packaged Desktop Runtime`
  - executable
  - cooked assets
  - runtime config

- `Hosted Session Runtime`
  - headless executable or container image
  - cooked assets
  - session bootstrap config
  - trust profile

### 23.2 Shared Runtime Requirement
Both outputs should share:

- simulation code
- asset format assumptions
- scripting API surface
- object identity model

### 23.3 Session Bootstrap Config
Hosted runtimes will need launch descriptors such as:

- project/build identifier
- asset manifest location
- trust profile
- requested runtime mode
- network endpoint config
- video streaming settings

## 24. Proposed Public Interfaces and Core Types

### 24.1 Rendering and Streaming
- `IRenderSurface`
- `IFrameCapture`
- `IVideoEncoder`

### 24.2 Session Transport
- `ISessionTransport`
- WebRTC transport implementation

### 24.3 Collaboration and Editor Authority
- `EditorSessionState`
- `EditorCommand`
- `EditorEvent`
- `EditorLock`
- `PresenceState`
- `SessionUserId`
- `SessionId`

### 24.4 Reflection and Assets
- `PropertyType`
- `PropertyDescriptor`
- `ObjectSchema`
- `AssetTypeDescriptor`
- `AssetId`
- `ObjectId`
- `EntityId`
- `ComponentId`

### 24.5 Scripting
- script lifecycle hooks
- sandbox/trust profile APIs
- engine-to-managed bridge contracts

### 24.6 Packaging and Deployment
- project manifest
- cooked asset manifest
- session launch config
- trust profile descriptor

## 25. Browser Application Structure
The web app should be structured as a true editor shell rather than a thin viewer.

Suggested modules:

- auth/session pages
- project browser
- editor workspace layout
- outliner
- inspector
- asset/content browser
- viewport panel
- collaboration/presence panel
- logs/console
- build/deploy flows

The viewport component should be treated as one subsystem among many, not the whole editor.

## 26. Operational Concerns

### 26.1 Session Lifecycle
The control service should manage:

- session creation
- reconnect
- handoff to engine runtime
- idle timeout
- cleanup
- crash reporting

### 26.2 Save Semantics
Project save behavior should be explicit:

- manual save remains available
- autosave policy can be added later
- hosted sessions should not rely on process lifetime for persistence

### 26.3 Failure Handling
The design should define behavior for:

- browser reconnect after transient disconnect
- session runtime crash
- lock owner disconnect
- failed asset import
- invalid command replay
- script exceptions

## 27. Security Considerations
- hosted scripting must be sandboxed
- session transport must authenticate participants
- lock and mutation permissions must be validated server-side
- project/session access control must not rely on client honesty
- browser UI should not be able to bypass engine authority through hidden RPCs
- encoded frame transport should be session-bound and expirable

## 28. Rollout Strategy

### Phase 0: Architectural Preparation
- formalize runtime modes
- define surface/capture/transport interfaces
- introduce core identity and reflection foundations
- keep native editor path working

Progress update:

- completed for the current seam-restoration subphase
- core session identity types and a local command/event seam now exist
- native editor path still works as the first local adapter
- runtime modes, render surfaces, and endpoint-oriented frame output are now wired back together
- `AxiomHeadless` is working as a command-driven headless runtime prototype
- `ISessionTransport` now formalizes the engine-facing remote boundary
- `AxiomRemoteViewportDevClient` remains available as a transport debug harness
- `AxiomRemoteViewportServer` now proves browser-driven remote viewport control against the same authoritative session seam over a native macOS WebRTC plus H.264 path
- the temporary JPEG fallback path has been removed from the remote viewport server
- encoded video packet publication and macOS H.264 encode now exist as part of the active browser viewport path rather than only as additive diagnostics seams
- `EditorFrontend` now exists as the real browser editor shell and owns the live WebRTC viewport client
- WebRTC sender/playout latency tuning is now the highest-priority remaining item in the remote viewport slice

### Phase 1: Remote Viewport Foundation
- support headless or offscreen rendering
- add frame capture
- add H.264 encode path
- connect a browser viewport over WebRTC
- support camera control and basic input

This is now the next major step. The local authoritative camera/input seam exists, so the next work should make that same session drive a headless/offscreen runtime and expose it through transport rather than only a local GLFW adapter.

Subphase status update:

- the seam-restoration slice is complete
- headless startup now works again without requiring a presentation surface
- the local editor still uses `WindowInputPlatform + GlfwEditorInputSource`
- offscreen frame publication is routed through `AxiomSessionEndpoint` rather than being hard-wired only to renderer-local capture polling
- the current dev-client slice now proves the transport seam with an in-process subscriber harness
- the current slice has replaced the dev harness as the main demo path
- encoded H.264 diagnostics are now exposed through the current server even though the browser viewport now uses the native WebRTC path
- validation on macOS should treat sandboxed media capability checks with caution because the sandbox can hide VideoToolbox encoder availability even when the machine supports H.264 encode
- the browser client now lives in the existing `EditorFrontend` application, specifically its viewport component and related browser-shell plumbing
- the next slice should prioritize deeper WebRTC transport-quality tuning rather than investing in retired server-hosted UI or removed fallback media paths

The first implementation step inside that phase is the `GLFW split`:

- `Wraith Engine` remains the engine and runtime product name
- `Axiom` names the remote/distributed subsystem and session endpoint path
- GLFW should become a local adapter for input and presentation rather than the authority boundary for editor behavior

### Phase 2: Browser Editor Shell
- build browser editor layout
- scene tree
- inspector
- selection
- logs
- project/session joining

Current implementation note:

- this phase should start in a new root-level `EditorFrontend` folder
- `EditorFrontend` is the home for the React/Next.js/Tailwind editor shell and has replaced the temporary browser UI that used to be served by `AxiomRemoteViewportServer`

### Phase 3: Authoritative Editing
- introduce command/event model
- add `EditorSessionState`
- support selection, transform, rename, reparent, component/property edits

Progress update:

- command/event model is in place and proven through 30+ focused tests
- `EditorSessionState` covers viewport camera, look state, selection, presence, scene items, object details, mesh instances, and object collaboration state
- selection, transform, rename, and visibility are all implemented as validated authoritative commands
- object lifecycle (create/duplicate/delete) is now implemented for a narrow built-in type set (`Folder`, `Mesh`, `Light`, `Camera`, `Actor`) in-memory without asset-browser integration
- all scene objects are now backed by a `DataModel`-rooted Instance hierarchy; the `EditorSession` owns the live tree and keeps the serializable `Items` projection in sync
- deep subtree duplication generates unique IDs and display names for every descendant
- reparent and component/property edits remain future work

### Phase 4: Collaboration v1
- presence
- user cameras/avatars
- selection visibility
- object/asset locking

Progress update:

- `EditorObjectLockState` (`Unlocked` / `Locked`) and `EditorObjectCollaborationState` are now part of `EditorSessionState`; lock state is per-object and per-user
- `AcquireLock`, `ReleaseLock`, and `ReleaseAllLocksForUser` are implemented as public `EditorSession` methods that publish `ObjectLockChangedEvent` to all clients on every state change
- gizmo drag start acquires the lock for the dragging user; drag end releases it; hard disconnect and presence timeout both call `ReleaseAllLocksForUser` so stale locks cannot persist
- `ValidateCommand` rejects `SetTransform`, `Rename`, `SetObjectVisibility`, `Delete`, and `Reparent` on any object locked by a different user
- `ObjectLockChangedEvent` is serialized as `object_lock_changed` and handled in the browser to maintain a `lockedObjects` map in `RemoteViewportContext`
- the outliner shows per-row avatar chips for each user whose selection matches that object, and a lock icon in the locking user's color when the object is locked
- `PresenceRoster` component renders a strip of avatar chips with colored presence-state dots (green / yellow / grey) mounted in the editor toolbar
- heartbeat protocol: the browser sends `{ "type": "heartbeat" }` every 4 s; the server resets `LastActivity` and promotes Away → Connected on receipt
- `PresenceLoop` background thread transitions Connected → Away at 10 s and Away → Disconnected at 30 s of inactivity, releasing all locks at the Disconnected transition
- `presentationColor` is assigned server-side per `SessionUserId` and used consistently for avatar chips, lock icons, and the presence roster

### Phase 5: Reflection and Asset Evolution
- property metadata system
- asset IDs
- metadata persistence
- first custom cooked asset containers

Progress update:

- `AssetId` stable identity type added to `Axiom/Session/SessionTypes.h`, mirroring `SessionUserId` / `SessionUserIdHash`
- `IAssetSource` / `LocalAssetSource` VFS abstraction introduced in `Axiom/Assets/IAssetSource.h/.cpp`; `LocalAssetSource` scans a root directory and derives stable `AssetId` values from hashed relative paths; `ResolveRelative` provides typed engine-internal path lookup
- `ListAssets`, `GetSchema`, `SetProperty`, and `SaveScene` command types added to `HeadlessCommandType` and `ParseRemoteViewportCommand`; all four are handled in both the WebSocket and WebRTC dispatch paths in `RemoteViewportServer`
- `SerializeAssetList`, `SerializeObjectSchema`, and `SerializeSaveResult` serializers added to `HeadlessCommandProtocol`
- `SceneFile` serializer/parser implemented in `Axiom/Assets/SceneFile.h/.cpp` using a manual ostringstream JSON emitter and a purpose-built recursive descent parser; flat-node format with `parentId` links avoids any external JSON library dependency
- `LoadStartupScene` now checks for `Content/scene.json` first and falls back to the hardcoded default startup scene, making scene state persistent across restarts
- `SaveScene` command writes `Content/scene.json` at runtime and returns `scene_saved` or `scene_save_failed`; the pre-existing toolbar Save button is now wired end-to-end and animates to a green checkmark (success) or red X (failure) for 2.5 s
- content browser replaced with a live server-driven implementation: `listAssets` is dispatched on connection, results populate grid/list views with mesh/texture filter tabs and a Refresh button
- details panel now fetches schema dynamically via `getSchema` on each selection change; `className` badge is shown in the panel header; property `readOnly` flags gate transform editing
- `SetProperty` dispatches per-property to the appropriate existing command (`RenameObject`, `SetObjectVisibility`, or `SetTransform`); vec3 properties patch the current transform so only the changed axis changes
- cooked asset containers remain future work; `IAssetSource` and `AssetId` establish the identity foundation they will build on

### Phase 6: C# Scripting
- engine API assembly
- host bridge
- hosted restricted profile
- trusted local profile

Progress update:

- `ScriptHost` initializes a Coral `HostInstance` and owns two Assembly Load Contexts: `WraithEngine` (engine API) and `UserScripts` (user code, collectible/reloadable)
- `WraithEngine.Managed` C# assembly exposes `Script` (base class with `OnCreate`, `OnTick`, `OnDestroy`), `GameObject` (object handle with `GetName`, `GetTransform`, `SetTransform`, `GetVisible`), `Transform`, and `Vector3`
- `InternalCalls` binds C++ `EditorSession` methods to the managed surface via Coral's unmanaged function pointer fields; `RegisterInternalCalls` uploads them to the engine ALC
- `ScriptHost` implements `IEditorEventSubscriber`; `ObjectCreatedEvent` instantiates scripts, `ObjectDeletedEvent` destroys them, `ScriptClassChangedEvent` handles attach/detach
- `ScriptHost::Tick(dt)` drives `OnTick` on all live instances; `ScriptHost::ReloadUserAssembly` tears down and recreates the `UserScripts` ALC and re-instantiates all scripts
- macOS `kqueue`-based file watcher auto-triggers reload when the assembly on disk changes (behind `AXIOM_SCRIPTING_WATCH` flag)
- Two trust tiers: `Restricted` (default for hosted — blocks `System.Net.*`, `System.Reflection.Emit`, `System.Diagnostics.Process`; assembly manifest validated via `PEReader` before loading) and `Trusted` (local dev — full BCL)
- `RestrictedAssemblyLoadContext` and `TrustedAssemblyLoadContext` enforce the policy in managed code; `ScriptSecurity` bridges the `IsRestricted` flag via an internal call
- Browser: details panel shows a `ScriptClass` text field; toolbar Reload Scripts button sends `reload_scripts`; unhandled script exceptions surface as dismissible toasts
- Five Google Test cases in `Tests/ScriptingTests.cpp`: `ScriptHostLifecycle`, `InternalCallRoundTrip`, `ScriptLifecycle`, `HotReload`, `RestrictedProfileBlocks`
- Coral patched: cross-ALC assembly sharing in `AssemblyLoader.ResolveAssembly` (prevents duplicate `WraithEngine.Managed` load with null function pointers) and `ManagedAssembly::RefreshTypeCache` (repopulates `s_CachedTypes` after `UnloadAssemblyLoadContext` clears it globally)

### Phase 7: Asset Pipeline — Textures, FBX/OBJ Import, Basic Materials

Scope: Expand the asset system beyond raw `.glb` mesh loading to support standalone
texture import, FBX and OBJ source formats, and a first-pass material model that wires
surface parameters to the Vulkan renderer without requiring a full PBR implementation.

#### 7.1 Texture Import
- Add `TextureAsset` to the `IAssetSource` / `AssetId` identity model
- Import `.png`, `.jpg`, `.jpeg`, `.hdr`, `.exr` via stb_image or a similar single-header library
- Upload to Vulkan via a staging buffer / `vkCmdCopyBufferToImage` path; cache as `VkImage` + `VkImageView` + `VkSampler`
- Surface texture assets in the content browser under the existing Texture filter tab (already present in the UI)
- Add `TextureRef` property kind to the reflection system so materials can reference textures by `AssetId`

#### 7.2 FBX / OBJ Import
- Add [assimp](https://github.com/assimp/assimp) as a ThirdParty dependency (CMake `FetchContent` or submodule)
- Introduce `IAssetImporter` interface with implementations for FBX (via assimp) and OBJ (via assimp or tinyobjloader)
- Emit glTF-compatible in-memory mesh and material data so the existing `fastgltf` render path can be reused downstream; alternatively introduce a shared `MeshData` struct consumed by both loaders
- Register `.fbx` and `.obj` extensions in `LocalAssetSource` and the content browser filter tabs
- Import settings (coordinate-system handedness flip, scale factor, smoothing groups) stored as metadata alongside the stable `AssetId`

#### 7.3 Basic Materials (no PBR required)
- Introduce `MaterialAsset` with at minimum: albedo color (`vec4`), albedo texture ref, roughness (`float`), metallic (`float`)
- Serialize material assets to `Content/` as `.mat.json` files alongside the stable `AssetId`
- Add a simple forward-shading GLSL material shader variant; the existing mesh pipeline switches to a per-material descriptor set
- Expose material properties in the details panel via `GetSchema` / `SetProperty` (reuses the existing reflection + property dispatch path)
- `MeshObject` gains a `MaterialId` reference; `SceneMeshObject` stores it; `SetProperty("materialId", ...)` dispatches a new `SetMaterialCommand`
- Full PBR (IBL, clearcoat, transmission) is deferred to a later rendering phase

#### 7.4 Lighting
- Add `LightComponent` as a first-class scene instance type alongside the existing `SceneLight` stub; light properties stored in `EditorObjectDetails`
- **Point light** — position, color, intensity, range (attenuation radius); represented as a uniform buffer entry in a per-frame light list
- **Directional light** — direction (derived from transform rotation), color, intensity; one active directional light per scene for v1
- **Spot light** — position, direction, color, intensity, inner/outer cone angles
- **Ambient** — a single ambient color/intensity applied to every fragment as a base term; simple sky color for v1 (IBL/HDRI deferred to a later PBR phase)
- Vulkan: extend the per-frame UBO (or add a separate SSBO) to carry a packed light list; update the forward-shading fragment shader to iterate lights using Blinn-Phong or a simple physically-motivated approximation
- **Shadow mapping** — directional shadow map first (single cascade, 2048×2048 depth attachment, PCF filter); point and spot shadow maps deferred to a later phase
- Light objects appear in the scene outliner, support transform editing, and expose their properties in the details panel via the existing `GetSchema` / `SetProperty` path
- `GetSchema` for a Light object returns `color`, `intensity`, `range`/`angle` (type-specific), and `castsShadows` as editable properties
- Emissive property on `MaterialAsset` (color + intensity multiplier) so meshes can self-illuminate without requiring a separate light source

### Phase 8: Binary Asset Formats

Scope: Define engine-owned binary container formats for each cooked asset class.  Every
asset type (mesh, texture, material) gets a single canonical on-disk representation
after import.  This decouples the runtime load path from source-format parsers, gives
all asset types a uniform structure, and is the prerequisite for packaging games — the
packaged runtime just ships the cooked binaries and a manifest, with no source files or
editor-only metadata included.

#### 8.1 Design Principles
- One binary format per asset class, version-tagged in the file header
- Header contains: magic bytes, format version, asset type enum, stable `AssetId` hash, content length
- Payload is asset-type-specific but follows a fixed schema (no external dependencies at load time)
- Editor-only data (import settings, source path, thumbnail cache) lives in a sidecar `.meta` JSON file, never in the cooked binary
- Packaged build strips all `.meta` files and source assets; the runtime only needs the cooked binaries and the asset manifest

#### 8.2 Format Definitions

**Mesh binary (`.wmesh`)**
- Header: magic `WMSH`, `uint32` version, `AssetId` (8 bytes), vertex count, index count, submesh count, flags
- Vertex buffer: interleaved `{vec3 position, vec3 normal, vec2 uv, vec4 tangent}` — layout fixed so the Vulkan pipeline never needs to inspect the source format
- Index buffer: `uint32[]` indices
- Submesh table: `{uint32 indexOffset, uint32 indexCount, AssetId materialId}[]`
- Bounding sphere and AABB stored after submesh table for culling; no re-parsing needed at runtime

**Texture binary (`.wtex`)**
- Header: magic `WTEX`, `uint32` version, `AssetId`, width, height, mip count, `VkFormat` enum value, flags (sRGB, normal map, HDR)
- Payload: raw mip chain in memory-layout order matching the target `VkFormat`; uploaded directly via a staging buffer with no format conversion at load time

**Material binary (`.wmat`)**
- Header: magic `WMAT`, `uint32` version, `AssetId`
- Fixed-layout property block: albedo `vec4`, roughness `float`, metallic `float`, emissive color `vec3`, emissive intensity `float`
- Texture reference table: `{uint8 slot, AssetId textureId}[]` — slots map to binding points in the material descriptor set layout

#### 8.3 Import Pipeline Integration
- `IAssetImporter::Import()` now produces the appropriate `*.wmesh` / `*.wtex` / `*.wmat` file in `Content/Cooked/` in addition to writing the `.meta` sidecar
- `LocalAssetSource` resolves `AssetId → cooked path` at runtime; source file paths are only consulted by the importer and editor tools, never by the renderer
- Re-import regenerates the cooked binary; the `AssetId` (derived from the relative source path hash) remains stable across re-imports
- `AssetCookManifest` (JSON alongside the cooked directory) maps `AssetId → {cookedPath, formatVersion, sourceHash}` for incremental cook decisions

#### 8.4 Packaged Build Impact
- The packager copies only `Content/Cooked/` and the `AssetCookManifest`; source assets, `.meta` files, and `scene.json` are excluded
- The runtime loader switches to `CookedAssetSource` (parallel to `LocalAssetSource`) which reads from the manifest and never touches source files
- This is the direct enabler for Phase 11 (Packaging and Hosted Runtime Maturation)

### Phase 9: Networking Refactor

Scope: Replace the current bespoke WebSocket/HTTP signaling and data-channel transport
stack with a well-maintained, battle-tested library that handles framing, TLS, and
connection management reliably at scale.  Two candidates are shortlisted; the final
choice should be made before implementation begins.

#### 8.1 Candidate Libraries

**Option A — [uWebSockets](https://github.com/uNetworking/uWebSockets)**
- Header-only C++ library; extremely low overhead per connection
- Native WebSocket and HTTP server in one; familiar API shape for the existing server code
- TLS via BoringSSL or OpenSSL; supports SSL_CTX injection
- No built-in reliability/ordering guarantees beyond what WebSocket provides; game-state
  replication would sit on top

**Option B — [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)**
- Valve's production-proven game transport (used in Steam networking)
- Provides reliable, unreliable, and ordered channels with integrated packet loss handling,
  jitter buffers, and encryption (AES-GCM / Curve25519)
- More infrastructure than uWebSockets but a better fit if gameplay replication is
  a near-term goal alongside editor transport
- Browser clients would need a WebSocket-to-GNS bridge or a separate signaling path

#### 8.2 Refactor Scope
- Introduce `IControlPlaneTransport` (HTTP + WebSocket for signaling, session lifecycle,
  and editor command/event channels) as a distinct interface from the existing
  `ISessionTransport` (viewport streaming + realtime per-frame data)
- Replace hand-rolled HTTP/WebSocket server code in `RemoteViewportServer` with the
  chosen library; keep `ISessionTransport` contract stable so callers are unaffected
- Migrate the reliable data channel (editor commands, presence, lock state) and
  the current HTTP polling endpoints to the new transport
- Add TLS termination support; the current plain-socket path is acceptable only for
  local development
- Regression: all existing end-to-end tests and browser editor interactions must
  pass without modification after the swap

### Phase 10: God Class Refactoring

Scope: Several classes have grown to 1,500 – 2,500 lines and now own multiple distinct
responsibilities.  This phase extracts those responsibilities into focused units before
the codebase becomes materially harder to extend.

#### 10.1 Candidates for refactoring (audit before Phase 10 begins)
Likely targets based on current trajectory:

| File | Approximate size | Concerns to extract |
|------|-----------------|---------------------|
| `Axiom/Session/EditorSession.cpp` | ~2,000 lines | Command dispatch, event publication, lock management, presence logic, schema generation |
| `Headless/RemoteViewportServer.cpp` | ~1,500 lines | HTTP routing, WebSocket framing, WebRTC signaling, command parsing, client lifecycle |
| `Headless/HeadlessCommandProtocol.cpp` | ~800 lines | Growing with every new command; serialization/deserialization should be generated or table-driven |

#### 10.2 Proposed splits

**`EditorSession`** → split into:
- `EditorSession` — thin coordinator; owns state, wires subsystems
- `CommandDispatcher` — validates and routes incoming commands
- `EventBroadcaster` — serializes and fans out authoritative events
- `LockManager` — manages object/asset lock lifecycle
- `PresenceTracker` — heartbeat, idle detection, state transitions

**`RemoteViewportServer`** → split into:
- `HttpRouter` — route table + handler registration
- `WebSocketServer` — connection lifecycle and framing (or replaced by Phase 9 library)
- `WebRtcSignalingHandler` — SDP/ICE exchange
- `ClientSessionManager` — per-client state, reconnect, cleanup
- `CommandParser` — delegates to `HeadlessCommandProtocol`

**`HeadlessCommandProtocol`** → move toward a table-driven or code-generated pattern:
- `CommandSerializer` — outbound event JSON
- `CommandDeserializer` — inbound command JSON
- Register new commands by adding a row to a dispatch table rather than growing `if/else` or `switch` chains

#### 10.3 Acceptance criteria
- No existing test regressions
- No single `.cpp` file exceeds 600 lines after the refactor
- Each new unit has its own header with a single-sentence doc comment stating its responsibility
- CI passes without modifications to test code

### Phase 11: Packaging and Hosted Runtime Maturation
- packaged desktop builds from cooked content (depends on Phase 8 binary asset formats)
- hosted session deployment descriptors
- sample project proving shared runtime path via `CookedAssetSource`

## 29. Test Plan

### 29.1 Headless Rendering
- render without a visible window
- capture frames successfully
- confirm local windowed rendering still works

### 29.2 Streaming
- browser connects and receives viewport stream
- browser input round-trips to authoritative camera movement
- transient disconnect/reconnect preserves authoritative state

### 29.3 Collaboration
- two users join the same session
- both users see presence and camera updates
- object lock prevents conflicting edits
- presence updates do not incorrectly mutate world state

### 29.4 Reflection and Assets
- reflected schemas serialize and deserialize consistently
- asset IDs remain stable across save/load/cook
- property edits from the browser map correctly to native runtime objects

### 29.5 Scripting
- hosted restricted mode blocks forbidden access
- trusted local mode can enable expanded features by explicit config
- script API version mismatch fails clearly

### 29.6 Packaging
- one sample project runs as a packaged desktop build and as a hosted authoritative runtime from the same cooked project content

### 29.7 Regression Coverage
- native application loop still supports desktop editor startup
- renderer still accepts scene submissions cleanly
- existing asset loading path remains usable until replaced

## 30. Risks and Tradeoffs

### 30.1 Main Risks
- headless Vulkan and frame capture complexity
- transport complexity and browser media edge cases
- accidental duplication of editor logic across browser and native clients
- over-designing collaboration before the authoritative command model is stable
- scripting sandbox complexity
- asset pipeline churn if identity/reflection rules are introduced too late

### 30.2 Main Tradeoffs
- WebRTC adds complexity but is the right latency fit
- browser-first editor UI reduces native duplication but increases frontend scope
- server authority improves consistency and collaboration but raises infra cost
- hybrid assets improve ergonomics but require a more deliberate pipeline
- trust tiers preserve flexibility without sacrificing hosted safety

## 31. Recommended First Implementation Slice
The first implementation slice after this design doc should be:

1. runtime mode abstraction
2. offscreen render surface
3. frame capture interface
4. session transport interface with a WebRTC-shaped contract
5. browser viewport prototype
6. authoritative per-user viewport camera state

Progress update:

- item 6 is now implemented locally
- items 1 through 5 now exist in prototype form and are wired through the same session authority seam rather than bypassing it
- the remote viewport prototype has already pivoted from shared-frame multiplexing to per-client render views with WebRTC-only streaming
- the authoritative scene-authoring loop has advanced: selection, transform, rename, visibility, and full object lifecycle (create/duplicate/delete) are all command-driven with event publication, test coverage, and end-to-end wiring through the browser editor shell
- all scene objects are now backed by a Roblox-inspired `DataModel`-rooted Instance hierarchy; the session owns the live tree and exposes a serializable snapshot projection for consumers
- the outliner exposes the full object lifecycle to the user: an Add dropdown with built-in templates, a Delete toolbar button, and a right-click context menu with Duplicate and Delete; the three operations travel the same WebRTC/HTTP → `ParseRemoteViewportCommand` → `EditorSession::Tick` → event broadcast path as every other authoritative command
- in-outliner inline rename is now implemented: double-clicking any object name in the outliner opens an in-place input that commits on Enter or blur and cancels on Escape; it drives the existing `rename_object` command path without any new C++ or protocol work
- the details panel supports rename and transform editing; drafts are scoped to the selected object's ID so periodic server snapshot polls do not clobber edits in progress
- viewport keyboard input (WASD, Space, Shift) is now gated on pointer lock state; keys are only consumed while the viewport has pointer lock and are cleared immediately when it releases, so other UI elements (inputs, the outliner) receive input normally
- a server-side transform gizmo is now implemented across all three modes (Translate, Scale, Rotate); the toolbar Move/Rotate/Scale buttons and Q/E/R shortcuts switch modes with active-state feedback; dragging any handle drives `SetTransformCommand` through the same authoritative command path
- reparent is implemented: any object can be dragged onto any other in the outliner; transforms are stored in local space and world transforms are recomputed for the entire moved subtree
- Collaboration v1 is complete: object locking prevents simultaneous gizmo conflicts, presence roster shows connected users, and the heartbeat/timeout loop handles hard tab closes
- Phase 5 (Reflection and Asset Evolution) is complete: `AssetId` stable identity, `IAssetSource` / `LocalAssetSource` VFS, `ListAssets` / `GetSchema` / `SetProperty` / `SaveScene` commands, `SceneFile` JSON persistence, content browser wired to live asset catalogue, details panel schema-driven, toolbar Save button with success/failure animation
- the next step is Phase 6: C# scripting host and engine API assembly

That slice proves the core thesis:

- the engine can run remotely
- the browser can drive it
- the same runtime can remain useful locally

It also avoids prematurely implementing the hardest later-stage systems such as full asset reflection, distributed save pipelines, or sandboxed script execution before the remote editor loop exists.

## 32. Final Recommendation
WraithEngine should be built as a distributed platform anchored on a single authoritative native runtime, not as a website that happens to show frames from a renderer. The browser editor, hosted runtime, collaboration layer, scripting host, and asset system should all be designed around that shared authority model from the beginning.

The first milestone should stay disciplined:

- browser-based editor shell
- streamed native viewport
- authoritative engine session
- single-user authoritative scene authoring before broader collaboration depth
- collaboration via presence and locking
- clear trust boundaries

If those pieces are designed cleanly, local packaging, hosted game sessions, richer scripting, and future collaboration depth can all grow from the same foundation instead of becoming parallel products.
