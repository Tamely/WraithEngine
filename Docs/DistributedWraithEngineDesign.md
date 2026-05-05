# Distributed Wraith Engine Design

## Document Status
- Status: Draft
- Date: 2026-05-05
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
- Added `AxiomRemoteViewportServer` as the first real browser-facing remote viewport prototype, first using HTTP plus image polling and now using WebSocket plus JPEG frame push
- The WebSocket plus JPEG slice improves the prototype substantially, but it is still too choppy for the target remote-editor experience
- WebRTC plus H.264 is now the highest-priority next transport step
- The long-lived browser editor UI should move into a root-level `EditorFrontend` workspace using React, Next.js, and Tailwind CSS instead of continuing to grow only inside the temporary localhost prototype

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

- add a root-level `EditorFrontend` folder as the home for the browser editor shell
- treat the current localhost viewport page served by `AxiomRemoteViewportServer` as a temporary bring-up client that can be migrated or retired once `EditorFrontend` is in place

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

- the initial local slice currently covers per-user viewport camera state, look/cursor-capture state, last cursor position bookkeeping, and authoritative scene submissions for the startup scene
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

- `ViewportCameraUpdated`
- `LookStateChanged`
- `CommandRejected`

are now implemented locally as the first authoritative session events

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
- `AxiomRemoteViewportServer` now proves browser-driven remote viewport control against the same authoritative session seam
- the current WebSocket plus JPEG transport is still visibly choppy and should not be treated as the endpoint architecture
- WebRTC/H.264 transport and the broader browser editor shell are now the highest-priority remaining items in the remote viewport slice

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
- the current localhost:8080 browser client is still a temporary prototype and should be migrated into a root-level `EditorFrontend` application as the browser shell work begins
- the next slice should prioritize WebRTC plus H.264 transport quality rather than continuing to invest heavily in the temporary JPEG-streaming path

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
- `EditorFrontend` should be the home for the React/Next.js/Tailwind editor shell and should eventually absorb or replace the temporary browser UI currently served by `AxiomRemoteViewportServer`

### Phase 3: Authoritative Editing
- introduce command/event model
- add `EditorSessionState`
- support selection, transform, rename, reparent, component/property edits

### Phase 4: Collaboration v1
- presence
- user cameras/avatars
- selection visibility
- object/asset locking

### Phase 5: Reflection and Asset Evolution
- property metadata system
- asset IDs
- metadata persistence
- first custom cooked asset containers

### Phase 6: C# Scripting
- engine API assembly
- host bridge
- hosted restricted profile
- trusted local profile

### Phase 7: Packaging and Hosted Runtime Maturation
- packaged desktop builds from cooked content
- hosted session deployment descriptors
- sample project proving shared runtime path

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
- the next slice should focus on items 1 through 5 in a way that reuses the new local session authority seam instead of bypassing it
- the immediate priority inside items 3 through 5 is now WebRTC plus H.264 transport, not further iteration on the temporary JPEG-streaming localhost page

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
- collaboration via presence and locking
- clear trust boundaries

If those pieces are designed cleanly, local packaging, hosted game sessions, richer scripting, and future collaboration depth can all grow from the same foundation instead of becoming parallel products.
