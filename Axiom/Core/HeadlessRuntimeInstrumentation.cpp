#include "Core/HeadlessRuntimeInstrumentation.h"

#include "Core/Log.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace Axiom {
namespace {
#if !defined(NDEBUG)
struct HeadlessRuntimeInstrumentationState {
  uint64_t EngineTickCount{0};
  uint64_t TotalRenderPasses{0};
  uint64_t LastTickRenderPassCount{0};
  size_t ActiveRemoteClientCount{0};
  size_t PendingOffscreenReadbacks{0};
  uint64_t TotalOffscreenReadbacksSubmitted{0};
  uint64_t TotalOffscreenReadbacksCompleted{0};
  uint64_t LastLoggedRenderPassCount{0};
  size_t LastLoggedRemoteClientCount{0};
  size_t LastLoggedPendingReadbacks{0};
  std::unordered_map<std::string, HeadlessClientCadenceSnapshot> ClientCadence;
};

HeadlessRuntimeInstrumentationState &GetState() {
  static HeadlessRuntimeInstrumentationState State;
  return State;
}

std::mutex &GetStateMutex() {
  static std::mutex Mutex;
  return Mutex;
}

std::string MakeCadenceKey(const std::string &ClientId, SessionUserId User,
                           bool IsLocal) {
  if (IsLocal || ClientId.empty()) {
    return "__local__:" + std::to_string(User.Value);
  }
  return ClientId;
}

bool ShouldLogSummary(const HeadlessRuntimeInstrumentationState &State) {
  return State.EngineTickCount <= 1u ||
         State.LastTickRenderPassCount != State.LastLoggedRenderPassCount ||
         State.ActiveRemoteClientCount != State.LastLoggedRemoteClientCount ||
         State.PendingOffscreenReadbacks != State.LastLoggedPendingReadbacks ||
         (State.EngineTickCount % 120u) == 0u;
}

void EnsureLoggingInitialized() {
  static std::once_flag Flag;
  std::call_once(Flag, []() { Log::Init(); });
}
#endif
} // namespace

void HeadlessRuntimeInstrumentation::Reset() {
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  GetState() = {};
#endif
}

void HeadlessRuntimeInstrumentation::RecordHeadlessTick(
    uint64_t EngineTick, size_t RenderPassCount, size_t ActiveRemoteClientCount) {
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  auto &State = GetState();
  State.EngineTickCount = EngineTick;
  State.LastTickRenderPassCount = RenderPassCount;
  State.TotalRenderPasses += RenderPassCount;
  State.ActiveRemoteClientCount = ActiveRemoteClientCount;
  if (ShouldLogSummary(State)) {
    EnsureLoggingInitialized();
    A_CORE_INFO(
        "HeadlessRuntime: tick={} render_passes={} active_remote_clients={} "
        "pending_readbacks={} total_render_passes={}",
        State.EngineTickCount, State.LastTickRenderPassCount,
        State.ActiveRemoteClientCount, State.PendingOffscreenReadbacks,
        State.TotalRenderPasses);
    State.LastLoggedRenderPassCount = State.LastTickRenderPassCount;
    State.LastLoggedRemoteClientCount = State.ActiveRemoteClientCount;
    State.LastLoggedPendingReadbacks = State.PendingOffscreenReadbacks;
  }
#else
  (void)EngineTick;
  (void)RenderPassCount;
  (void)ActiveRemoteClientCount;
#endif
}

void HeadlessRuntimeInstrumentation::RecordHeadlessRenderPass(
    uint64_t EngineTick, size_t PassIndex, const std::string &ClientId,
    SessionUserId User, bool IsLocal) {
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  auto &State = GetState();
  HeadlessClientCadenceSnapshot &Client =
      State.ClientCadence[MakeCadenceKey(ClientId, User, IsLocal)];
  if (Client.RenderPassCount == 0) {
    Client.ClientId = ClientId;
    Client.User = User;
    Client.IsLocal = IsLocal;
  }

  const uint64_t PreviousTick = Client.LastEngineTick;
  ++Client.RenderPassCount;
  Client.LastEngineTick = EngineTick;
  if (PreviousTick > 0u && EngineTick > PreviousTick) {
    Client.LastTicksSincePreviousRender = EngineTick - PreviousTick;
    Client.MaxTicksBetweenRenders = std::max(Client.MaxTicksBetweenRenders,
                                             Client.LastTicksSincePreviousRender);
  }

  EnsureLoggingInitialized();
  A_CORE_TRACE(
      "HeadlessRuntime: tick={} pass={} client='{}' user={} local={} "
      "client_render_passes={}",
      EngineTick, PassIndex, ClientId.empty() ? "<local>" : ClientId, User.Value,
      IsLocal ? "true" : "false", Client.RenderPassCount);
#else
  (void)EngineTick;
  (void)PassIndex;
  (void)ClientId;
  (void)User;
  (void)IsLocal;
#endif
}

void HeadlessRuntimeInstrumentation::RecordPendingOffscreenReadbacks(
    size_t PendingReadbacks) {
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  GetState().PendingOffscreenReadbacks = PendingReadbacks;
#else
  (void)PendingReadbacks;
#endif
}

void HeadlessRuntimeInstrumentation::RecordOffscreenReadbackSubmitted(
    uint64_t FrameNumber, SessionUserId User, size_t PendingReadbacks) {
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  auto &State = GetState();
  ++State.TotalOffscreenReadbacksSubmitted;
  State.PendingOffscreenReadbacks = PendingReadbacks;
  EnsureLoggingInitialized();
  A_CORE_TRACE(
      "HeadlessRuntime: submitted offscreen readback frame={} user={} "
      "pending_readbacks={}",
      FrameNumber, User.Value, State.PendingOffscreenReadbacks);
#else
  (void)FrameNumber;
  (void)User;
  (void)PendingReadbacks;
#endif
}

void HeadlessRuntimeInstrumentation::RecordOffscreenReadbackCompleted(
    uint64_t FrameNumber, SessionUserId User, size_t PendingReadbacks) {
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  auto &State = GetState();
  ++State.TotalOffscreenReadbacksCompleted;
  State.PendingOffscreenReadbacks = PendingReadbacks;
  EnsureLoggingInitialized();
  A_CORE_TRACE(
      "HeadlessRuntime: completed offscreen readback frame={} user={} "
      "pending_readbacks={}",
      FrameNumber, User.Value, State.PendingOffscreenReadbacks);
#else
  (void)FrameNumber;
  (void)User;
  (void)PendingReadbacks;
#endif
}

HeadlessRuntimeInstrumentationSnapshot
HeadlessRuntimeInstrumentation::GetSnapshot() {
  HeadlessRuntimeInstrumentationSnapshot Snapshot{};
#if !defined(NDEBUG)
  std::scoped_lock Lock(GetStateMutex());
  const auto &State = GetState();
  Snapshot.EngineTickCount = State.EngineTickCount;
  Snapshot.TotalRenderPasses = State.TotalRenderPasses;
  Snapshot.LastTickRenderPassCount = State.LastTickRenderPassCount;
  Snapshot.ActiveRemoteClientCount = State.ActiveRemoteClientCount;
  Snapshot.PendingOffscreenReadbacks = State.PendingOffscreenReadbacks;
  Snapshot.TotalOffscreenReadbacksSubmitted =
      State.TotalOffscreenReadbacksSubmitted;
  Snapshot.TotalOffscreenReadbacksCompleted =
      State.TotalOffscreenReadbacksCompleted;
  Snapshot.ClientCadence.reserve(State.ClientCadence.size());
  for (const auto &[Key, Client] : State.ClientCadence) {
    (void)Key;
    Snapshot.ClientCadence.push_back(Client);
  }
  std::sort(Snapshot.ClientCadence.begin(), Snapshot.ClientCadence.end(),
            [](const HeadlessClientCadenceSnapshot &Left,
               const HeadlessClientCadenceSnapshot &Right) {
              if (Left.IsLocal != Right.IsLocal) {
                return Left.IsLocal < Right.IsLocal;
              }
              if (Left.ClientId != Right.ClientId) {
                return Left.ClientId < Right.ClientId;
              }
              return Left.User.Value < Right.User.Value;
            });
#endif
  return Snapshot;
}
} // namespace Axiom
