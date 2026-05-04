#pragma once

#include <cstdint>

namespace Axiom {
class Window;
class RenderScene;

enum class RendererBackendType { Vulkan };

struct RendererCreateInfo {
  Window *TargetWindow{nullptr};
  uint32_t Width{0};
  uint32_t Height{0};
  RendererBackendType BackendType{RendererBackendType::Vulkan};
};

struct RenderFrameInfo {
  uint64_t FrameIndex{0};
};

class RendererBackend {
public:
  virtual ~RendererBackend() = default;

  virtual void Init(const RendererCreateInfo &CreateInfo) = 0;
  virtual void Shutdown() = 0;
  virtual void BeginFrame() = 0;
  virtual void RenderFallbackBackground(RenderScene &Scene) = 0;
  virtual void RenderImGui() = 0;
  virtual void EndFrame() = 0;
};
} // namespace Axiom

