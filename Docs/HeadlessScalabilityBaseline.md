# Headless Scalability Baseline

This note captures where to read the Phase 0 / Phase 1 headless scalability counters while tuning scheduling and asynchronous readback behavior.

Phase 1 has landed: offscreen readbacks are now published after later fence polling
instead of waiting immediately after submit, and idle remote views are throttled by
dirty/burst scheduling. Treat this file as the current instrumentation guide, not
as a pre-Phase-1 TODO.

## Where The Counters Live

- `Axiom::HeadlessRuntimeInstrumentation` in [Axiom/Core/HeadlessRuntimeInstrumentation.h](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Core/HeadlessRuntimeInstrumentation.h)
- Headless render-pass scheduling hook in [Headless/HeadlessSessionHost.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Headless/HeadlessSessionHost.cpp)
- Offscreen readback hook in [Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp](/Users/joshua/Documents/GitHub/WraithEngine/Axiom/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp)

## What To Capture During Tuning

- `LastTickRenderPassCount`: current render passes scheduled for one engine tick.
- `TotalRenderPasses`: cumulative render-pass work over a benchmark window.
- `ActiveRemoteClientCount`: connected remote views currently driving scheduling.
- `PendingOffscreenReadbacks`: queued offscreen readbacks still waiting on fences.
- `TotalOffscreenReadbacksSubmitted` and `TotalOffscreenReadbacksCompleted`: whether readbacks are piling up or draining.
- `ClientCadence[*].RenderPassCount`, `LastTicksSincePreviousRender`, and `MaxTicksBetweenRenders`: per-client cadence and whether any client is already naturally throttled.

## How To Read Them

- In debug builds, `HeadlessSessionHost` emits periodic `HeadlessRuntime:` log lines when tick pass-count, remote client count, or pending readbacks change.
- In code, tests, or a debugger, call `Axiom::HeadlessRuntimeInstrumentation::GetSnapshot()` to read the full snapshot.
- Remote client/server totals are still available through `RemoteViewportServer::GetMetrics()`, but the render-pass and readback counters now live in the shared instrumentation snapshot.

## Recommended Baseline Runs

1. Single remote client connected and interacting with the viewport.
2. Multiple remote clients connected to the same scene.
3. One active client plus one mostly idle client.

For each run, record the snapshot after a fixed tick window and compare:

- render passes per tick against remote client count
- pending readbacks during steady state
- per-client cadence symmetry between active and idle clients

The old pre-Phase-1 baseline showed roughly one render pass per connected remote
client per engine tick. The current expected behavior is different: active or
recently mutated clients should receive a short full-rate burst, while idle
clients should settle into the scheduler's throttled cadence.
