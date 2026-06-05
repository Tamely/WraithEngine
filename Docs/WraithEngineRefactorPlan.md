# WraithEngine Refactor Plan

## Document Status
- Status: Draft
- Date: 2026-05-25
- Audience: Engine, rendering, headless runtime, and editor contributors
- Intended outcome: Turn the current engineering audit into an executable refactor roadmap ordered by dependency, risk, and team size

## Executive Summary

The current audit is still directionally correct, but the repo has evolved enough that the plan should target today's actual seams instead of the older architecture framing.

The most important current facts are:

- Scene authority already lives in editor-owned structs in `EditorSession`, but that data is mirrored into a recursive heap-owned `Instance` tree for hierarchy operations and projection.
- Render submission still carries `shared_ptr` ownership and still recovers backend-specific Vulkan types through `dynamic_cast` in the submission build path.
- Headless offscreen rendering still blocks on `vkWaitForFences` immediately after submit, which defeats frames-in-flight for the headless path.
- Multi-client headless rendering still performs one render pass per remote client per engine tick.
- `RemoteViewportServer` still mixes transport, WebRTC, project lifecycle, script workspace, asset upload, presence, input routing, and frame delivery in one class.
- String-keyed maps remain widespread in editor, headless, scripting, physics, and scene serialization paths even where stable integer handles would make the authority layer simpler and cheaper.

## Audit Validation

### 1. Scene and object model

Current implementation shape:

- Authoritative editor scene state lives in `EditorSessionState::Scene` in [Axiom/Session/EditorSession.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSession.h:130).
- Scene identity is string-based in `EditorSceneItem::Id`, `EditorObjectDetails::ObjectId`, selection state, collaboration state, and mesh instance ownership in [Axiom/Session/EditorSession.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSession.h:50).
- A parallel `Instance` tree still exists under `m_SceneRoot`, with raw parent and child pointers plus recursive ownership in [Axiom/CoreInstance/Instance.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/CoreInstance/Instance.h:42).
- Snapshot and edit operations rebuild or mutate that tree through `EditorSceneStateManager` in [Axiom/Session/EditorSceneStateManager.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSceneStateManager.cpp:173) and command handlers in [Axiom/Session/EditorCommandDispatcher.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorCommandDispatcher.cpp:236).

Validation:

- The old audit is still accurate about recursive ownership, heap chasing, and RTTI in the object model.
- The more precise problem today is not "scene state is only pointers"; it is "scene authority is duplicated across string-keyed value state and a pointer tree that still participates in core mutations."

### 2. Render submission

Current implementation shape:

- `EditorSceneRendererAdapter` rebuilds frame submissions from logical mesh instances and caches meshes by string object id in [Axiom/Session/EditorSceneRendererAdapter.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSceneRendererAdapter.cpp:8).
- `RenderMeshSubmission` still carries `MeshRef` plus a `VulkanMesh *TypedMesh` in [Axiom/Renderer/Mesh.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Mesh.h:81).
- `ResolveVulkanMesh` still uses `dynamic_cast` in [Axiom/Renderer/Mesh.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Mesh.cpp:41).
- Mesh creation still returns `std::shared_ptr<Mesh>` from renderer and backend interfaces in [Axiom/Renderer/Renderer.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Renderer.h:34) and [Axiom/Renderer/RendererBackend.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/RendererBackend.h:67).

Validation:

- This finding is fully accurate.
- The current build path is cleaner than earlier direct session-owned submissions, but the ownership model and backend recovery are still in the hot path.

### 3. Headless rendering and fences

Current implementation shape:

- Headless uses an offscreen render surface and publishes captured frames through the renderer frame-output seam in [Axiom/Core/Application.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Core/Application.cpp:152), [Axiom/Renderer/Vulkan/VulkanRendererBackend.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanRendererBackend.cpp:150), and [Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp:633).
- In the offscreen path, the draw submission system submits graphics work, marks the capture pending, then immediately waits on `CurrentFrame.RenderFence` before publishing the frame in [Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp:770).

Validation:

- This finding is fully accurate.
- The current implementation preserves frame attribution correctness, but it serializes headless rendering at the point where frames-in-flight should be helping.

### 4. Multi-client rendering

Current implementation shape:

- The application core supports multiple render passes per tick via `BeginRenderPasses()` in [Axiom/Core/Application.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Core/Application.cpp:152).
- `HeadlessSessionHost` builds one pass per active remote render view in [Headless/HeadlessSessionHost.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionHost.cpp:134).
- Each pass resolves a specific render user and repopulates the frame through `HeadlessSessionLayer::OnRender()` in [Headless/HeadlessSessionLayer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionLayer.cpp:31).

Validation:

- This finding is fully accurate.
- The engine currently re-renders once per active remote client per tick, even when the scene is shared and only camera/view overlays differ.

### 5. `RemoteViewportServer`

Current implementation shape:

- `RemoteViewportServer` owns the browser-facing server plus session transport subscriber behavior in [Headless/RemoteViewportServer.h](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.h:56).
- The same class handles HTTP routes, WebSocket messages, WebRTC offer and ICE endpoints, project create/open/cook/package, script CRUD, asset upload, presence tracking, per-client session creation, drag-and-drop commands, and frame routing in [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:1266), [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:1657), [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:2055), and [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:2268).

Validation:

- This finding is fully accurate.
- The recent module/runtime cleanup improved host seams around the server, but the server itself is still a major concentration point.

### 6. String-keyed state

Current implementation shape:

- Editor scene authority uses `unordered_map<string, ...>` for details and collaboration in [Axiom/Session/EditorSession.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSession.h:133).
- Headless remote view registry and remote clients are keyed by string client id in [Headless/HeadlessRenderView.h](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessRenderView.h:177) and [Headless/RemoteViewportServer.h](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.h:228).
- Script instances are keyed by string object id in [Axiom/Scripting/ScriptHost.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Scripting/ScriptHost.h:122).
- Serialization and runtime systems also keep string-keyed object maps in [Axiom/Assets/SceneFile.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Assets/SceneFile.cpp:704) and [Axiom/Physics/PhysicsWorld.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Physics/PhysicsWorld.cpp:172).

Validation:

- This finding is accurate, but not all string maps are equal.
- Client ids should remain externally meaningful strings.
- Scene authority and runtime-facing object identity are the stronger candidates for stable integer handles.

## Recommended Refactor Order

### Phase 1: Headless scalability slice

Why first:

- Highest current scalability payoff.
- Lowest semantic blast radius compared with scene storage rewrites.
- Directly addresses the clearest N-client cost center.

Target architecture:

- Offscreen rendering uses true asynchronous readback.
- Completed frames are published when fences signal on a later tick rather than by waiting immediately after submit.
- Headless render scheduling becomes policy-driven per view, with at least dirty-state or cadence-based throttling for inactive clients.

Migration strategy:

1. Remove the immediate fence wait from the headless offscreen path.
2. Let pending readbacks complete in later frames through the existing `PublishCompletedOffscreenFrames()` path.
3. Add headless counters for render-pass count, pending readbacks, and per-client frame cadence.
4. Add render scheduling policy in `HeadlessSessionHost` so all connected clients do not force full-rate rendering by default.

Files and systems affected:

- [Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp:633)
- [Axiom/Renderer/Vulkan/VulkanRendererBackend.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanRendererBackend.cpp:150)
- [Headless/HeadlessSessionHost.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionHost.cpp:134)
- [Headless/HeadlessViewportFrameBridge.h](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessViewportFrameBridge.h:11)

Risks:

- Stale or misattributed frames if readback ownership regresses.
- Increased capture latency if pending-frame queues are not bounded.
- Hidden coupling with encoder timing and WebRTC sender expectations.

Test strategy:

- Extend frame attribution tests around [Tests/LayerTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/LayerTests.cpp:940).
- Add a renderer-level regression test or harness for two queued offscreen frames with distinct users.
- Add instrumentation assertions in headless integration tests for render-pass count versus active-client count.

Incremental or staged:

- Incremental.

### Phase 2: Render submission cleanup

Why second:

- It compounds the Phase 1 gains.
- It reduces CPU overhead in the repeated per-client submission build path.
- It is more contained than scene-storage replacement.

Target architecture:

- Renderer submissions are plain data carrying opaque mesh/resource handles.
- Backend-specific type recovery happens inside the backend, not in upstream submission builders.
- Debug metadata is stable per resource or per object instead of being re-registered per frame.

Migration strategy:

1. Introduce an engine-owned mesh handle type beside `MeshRef`.
2. Teach renderer and backend interfaces to create and resolve handles without exposing Vulkan types.
3. Change `RenderMeshSubmission` to carry handle plus material/transform only.
4. Remove `TypedMesh` and retire `ResolveVulkanMesh` from the frame build path.

Files and systems affected:

- [Axiom/Renderer/Mesh.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Mesh.h:59)
- [Axiom/Renderer/Mesh.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Mesh.cpp:15)
- [Axiom/Renderer/Renderer.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Renderer.h:34)
- [Axiom/Renderer/RendererBackend.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/RendererBackend.h:67)
- [Axiom/Session/EditorSceneRendererAdapter.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSceneRendererAdapter.cpp:25)

Risks:

- Resource lifetime bugs if handle ownership is underspecified.
- Mesh cache invalidation bugs during asset reassignment.
- Render debug tooling regressions if debug ids stop matching submissions.

Test strategy:

- Keep visibility and transform-driven render behavior covered by [Tests/LayerTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/LayerTests.cpp:594).
- Add adapter cache tests for asset swap and deletion.
- Add a submission-shape test that no upstream code depends on `VulkanMesh *`.

Incremental or staged:

- Incremental.

### Phase 3: `RemoteViewportServer` decomposition

Why third:

- The class is already too broad for safe continued feature work.
- Splitting it after the first scalability slice avoids changing server boundaries while the rendering path is still shifting.

Target architecture:

- Thin server shell for HTTP and transport wiring.
- Extracted services for client registry, WebRTC/video streaming, project lifecycle, script workspace, asset service, and session command routing.
- Narrower lock scopes around client transport state versus project/session state.

Migration strategy:

1. Extract pure helper services without changing routes or wire format.
2. Introduce an internal command router so all browser command paths stop branching in one file.
3. Move project/script/asset HTTP behavior behind dedicated services.
4. Collapse repeated `ClientId` and `SessionUserId` resolution flows into one registry boundary.

Files and systems affected:

- [Headless/RemoteViewportServer.h](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.h:78)
- [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:1164)
- [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:1698)
- [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:2607)

Risks:

- Route regressions in browser workflows.
- Concurrency bugs if extracted services still share mutable state incorrectly.
- WebRTC session lifetime regressions during reconnect and disconnect flows.

Test strategy:

- Preserve protocol parse/serialize coverage in [Tests/HeadlessProtocolTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/HeadlessProtocolTests.cpp:83).
- Expand networking module tests in [Tests/WraithNetworkingModuleTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/WraithNetworkingModuleTests.cpp:73).
- Add reconnect/resume tests around session connect and WebRTC close semantics.

Incremental or staged:

- Incremental, but should be done as a series of service extractions rather than a single rewrite branch.

### Phase 4: Scene identity and storage redesign

Why fourth:

- This is the right long-term fix for scene authority, but it is not the cheapest way to buy scalability today.
- It has the broadest cross-cutting impact across editor, serialization, physics, scripting, and collaboration.

Target architecture:

- Stable integer handles for scene objects and hierarchy nodes inside the authority layer.
- Parent-child hierarchy stored as arrays or stable vectors rather than recursive heap objects.
- String ids preserved only for persistence, browser protocol compatibility, and user-facing labels where needed.
- `Instance` becomes an adapter-only construct or is removed from authoritative mutations entirely.

Migration strategy:

1. Introduce internal `SceneObjectHandle` alongside existing string object ids.
2. Build fast handle lookup tables in `EditorSession` and `EditorSceneStateManager`.
3. Migrate runtime-facing systems first: selections, collaboration state, mesh instance ownership, physics object binding, and script instance binding.
4. Move hierarchy operations to handle-based storage.
5. Reduce the `Instance` tree to a projection layer, then decide whether it still justifies its cost.

Files and systems affected:

- [Axiom/Session/EditorSession.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSession.h:121)
- [Axiom/Session/EditorSceneStateManager.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSceneStateManager.h:24)
- [Axiom/CoreInstance/Instance.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/CoreInstance/Instance.h:14)
- [Axiom/Session/EditorCommandDispatcher.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorCommandDispatcher.cpp:335)
- [Axiom/Assets/SceneFile.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Assets/SceneFile.cpp:704)
- [Axiom/Scripting/ScriptHost.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Scripting/ScriptHost.h:122)

Risks:

- Highest regression risk in the repo.
- Save/load, selection, duplicate, delete, reparent, physics, and scripting all depend on identity stability.
- Browser-facing protocols still expect string ids today, so translation layers must stay correct during migration.

Test strategy:

- Use current lifecycle coverage in [Tests/SceneLifecycleTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/SceneLifecycleTests.cpp:1300) as the baseline.
- Add handle-stability tests for duplicate, delete, reparent, and save/load round-trip.
- Add scripting and physics integration tests that prove handle remapping does not orphan runtime objects.

Incremental or staged:

- Staged rewrite.

## Prioritized Checklist

### Phase 0: Measurement and guardrails

- Add profiling counters for headless render passes, pending readbacks, and per-client frame cadence.
- Add a short benchmark harness for one client versus N clients in headless mode.
- Document baseline numbers before major refactors.

### Phase 1: Headless throughput

- Remove immediate offscreen `vkWaitForFences` from the render submit path.
- Publish completed offscreen captures on later ticks.
- Add per-client render scheduling policy so inactive clients do not all render at full cadence.
- Validate frame attribution and ordering after async readback.

### Phase 2: Render submission

- Introduce opaque mesh/resource handles.
- Remove `TypedMesh` from `RenderMeshSubmission`.
- Move backend-specific mesh resolution into the Vulkan backend.
- Stop creating render debug metadata on every submission build.

### Phase 3: Remote viewport server decomposition

- Extract client/session registry service.
- Extract project and script workspace services.
- Extract asset upload and listing service.
- Extract browser command router and reduce mutex scope.

### Phase 4: Scene identity and storage

- Add internal stable integer object handles.
- Move authority lookups and runtime bindings off string ids.
- Replace hierarchy mutations that depend on recursive `Instance *` traversal.
- Demote or remove the `Instance` tree from authoritative scene mutation paths.

## Incremental vs Staged

Changes that can be incremental:

- Headless async readback and render scheduling.
- Render submission handle cleanup.
- `RemoteViewportServer` service extraction.
- Narrow handle introduction under existing string ids.

Changes that require a staged rewrite:

- Replacing authoritative scene hierarchy storage.
- Removing `Instance` from core mutation paths.
- Converting all runtime-facing scene identity from strings to handles.

## Recommended First Slice

Start with headless scalability, not scene storage.

Minimal first slice:

1. Make offscreen frame capture truly asynchronous by removing the immediate fence wait in [Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp:782).
2. Keep current per-client render passes, but add simple render scheduling in [Headless/HeadlessSessionHost.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionHost.cpp:134) so unchanged or background clients do not all render every tick.
3. Instrument the runtime so the team can measure improvement before taking on bigger structural work.

Why this slice:

- It directly targets the two most obvious scalability bottlenecks.
- It avoids destabilizing scene editing, serialization, scripting, and collaboration semantics.
- It preserves the current browser/session protocol while improving the host runtime beneath it.

## Go / No-Go Guidance

Go:

- Start with headless multi-client scalability.
- Follow with render submission cleanup once the headless path is no longer serialized.

Conditional go:

- Begin `RemoteViewportServer` decomposition after the first scalability slice, especially if more browser-facing features are planned.

No-go:

- Do not start with scene storage as the first refactor.
- Do not start with a full multi-client redesign that assumes shared image reuse or compositor-level fanout before removing the current fence stall and measuring the real remaining cost.

## Evidence Sources

- [Axiom/Session/EditorSession.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSession.h:130)
- [Axiom/Session/EditorSceneStateManager.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSceneStateManager.cpp:173)
- [Axiom/CoreInstance/Instance.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/CoreInstance/Instance.h:42)
- [Axiom/Session/EditorCommandDispatcher.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorCommandDispatcher.cpp:335)
- [Axiom/Session/EditorSceneRendererAdapter.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Session/EditorSceneRendererAdapter.cpp:25)
- [Axiom/Renderer/Mesh.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Mesh.h:81)
- [Axiom/Renderer/Mesh.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Mesh.cpp:41)
- [Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp:770)
- [Axiom/Core/Application.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Core/Application.cpp:152)
- [Headless/HeadlessSessionHost.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionHost.cpp:134)
- [Headless/HeadlessSessionLayer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionLayer.cpp:31)
- [Headless/RemoteViewportServer.h](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.h:56)
- [Headless/RemoteViewportServer.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/RemoteViewportServer.cpp:1266)
- [Tests/LayerTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/LayerTests.cpp:594)
- [Tests/LayerTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/LayerTests.cpp:940)
- [Tests/SceneLifecycleTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/SceneLifecycleTests.cpp:1300)
- [Tests/HeadlessProtocolTests.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Tests/HeadlessProtocolTests.cpp:512)
