#pragma once

#include "Session/SessionTypes.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Axiom {
#if defined(NDEBUG)
inline constexpr bool kHeadlessRuntimeInstrumentationEnabled = false;
#else
inline constexpr bool kHeadlessRuntimeInstrumentationEnabled = true;
#endif

struct HeadlessClientCadenceSnapshot {
  std::string ClientId;
  SessionUserId User{};
  bool IsLocal{false};
  uint64_t RenderPassCount{0};
  uint64_t LastEngineTick{0};
  uint64_t LastTicksSincePreviousRender{0};
  uint64_t MaxTicksBetweenRenders{0};
};

struct HeadlessRuntimeInstrumentationSnapshot {
  bool Enabled{kHeadlessRuntimeInstrumentationEnabled};
  uint64_t EngineTickCount{0};
  uint64_t TotalRenderPasses{0};
  uint64_t LastTickRenderPassCount{0};
  size_t ActiveRemoteClientCount{0};
  size_t PendingOffscreenReadbacks{0};
  uint64_t TotalOffscreenReadbacksSubmitted{0};
  uint64_t TotalOffscreenReadbacksCompleted{0};
  std::vector<HeadlessClientCadenceSnapshot> ClientCadence;
};

class HeadlessRuntimeInstrumentation {
public:
  static constexpr bool IsEnabled() {
    return kHeadlessRuntimeInstrumentationEnabled;
  }

  static void Reset();
  static void RecordHeadlessTick(uint64_t EngineTick, size_t RenderPassCount,
                                 size_t ActiveRemoteClientCount);
  static void RecordHeadlessRenderPass(uint64_t EngineTick, size_t PassIndex,
                                       const std::string &ClientId,
                                       SessionUserId User, bool IsLocal);
  static void RecordPendingOffscreenReadbacks(size_t PendingReadbacks);
  static void RecordOffscreenReadbackSubmitted(uint64_t FrameNumber,
                                               SessionUserId User,
                                               size_t PendingReadbacks);
  static void RecordOffscreenReadbackCompleted(uint64_t FrameNumber,
                                               SessionUserId User,
                                               size_t PendingReadbacks);
  static HeadlessRuntimeInstrumentationSnapshot GetSnapshot();
};
} // namespace Axiom
