#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Axiom {
class Application;

enum class ModuleUpdatePhase {
  FrameStart,
  RenderBegin,
  Render,
  ImGuiRender,
  RenderEnd,
};

struct ModuleUpdateContext {
  Application &App;
  float DeltaTimeSeconds{0.0f};
  uint64_t FrameIndex{0};
  ModuleUpdatePhase Phase{ModuleUpdatePhase::FrameStart};
  size_t RenderPassIndex{0};
  size_t RenderPassCount{1};
};

class IModule {
public:
  virtual ~IModule() = default;

  [[nodiscard]] virtual std::string_view GetName() const = 0;
  virtual bool Initialize(Application &App) = 0;
  virtual void Update(const ModuleUpdateContext &Context) = 0;
  virtual void Shutdown(Application &App) = 0;
};
} // namespace Axiom
