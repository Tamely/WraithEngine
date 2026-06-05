# Headless Phase 1 Implementation Note

## Offscreen synchronization

- Headless offscreen rendering no longer waits on the submitted render fence immediately after `vkQueueSubmit2`.
- Each offscreen capture slot still records the submitted frame number and render user at submit time.
- Completed readbacks are polled from `PublishCompletedOffscreenFrames()` on later ticks.
- Ready capture slots are published in ascending submitted-frame order before the slot is reused, which preserves frame ordering even when multiple readbacks complete together.
- Reuse stays bounded by `FRAME_OVERLAP`: `PrepareFrame()` still waits before a command/fence slot is recycled, and the offscreen path asserts that the matching capture slot has been drained before recording over it.

## Frame publication

- Completed offscreen frames continue to flow through the existing viewport frame-output seam.
- The published `ViewportFrame` is tagged from the stored submit-time user, so attribution does not depend on whichever client happens to be active when the fence later signals.
- `ConsumeCapturedFrame()` now drains a small FIFO of completed captures instead of exposing only the most recent completion, which keeps multi-frame completion behavior deterministic for debugging and tests.

## Headless render scheduling

- Remote views now carry lightweight scheduling state: `NeedsRender` plus a short recent-activity burst window.
- New or mutated views are marked dirty and receive a brief full-rate burst.
- Shared scene activity marks all remote views dirty so collaborators get a prompt refresh after edits.
- Idle remote views are throttled to a round-robin cadence instead of forcing one render pass per client per engine tick by default.
- When no remote views exist, the local headless view remains the fallback render target as before.
