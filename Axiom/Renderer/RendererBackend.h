#pragma once

#include "Renderer/Mesh.h"

#include <glm/vec2.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Axiom {
class Window;
class RenderScene;
class IRenderSurface;
struct MeshData;

enum class RendererBackendType { Vulkan };
enum class RendererViewMode : uint32_t {
  Lit = 0,
  Unlit = 1,
  Wireframe = 2,
};

struct RendererCreateInfo {
  Window *TargetWindow{nullptr};
  std::shared_ptr<IRenderSurface> TargetSurface;
  uint32_t Width{0};
  uint32_t Height{0};
  RendererBackendType BackendType{RendererBackendType::Vulkan};
};

struct CapturedFrame {
  uint64_t FrameIndex{0};
  uint32_t Width{0};
  uint32_t Height{0};
  std::vector<std::byte> Pixels;
};

struct RenderFrameInfo {
  uint64_t FrameIndex{0};
};

struct RendererFrameStats {
  float CpuFrameMs{0.0f};
  float CpuRenderMs{0.0f};
  float GpuBackgroundMs{0.0f};
  float GpuMeshMs{0.0f};
  uint32_t SubmittedMeshCount{0};
  uint32_t FrustumCulledMeshCount{0};
  uint32_t OcclusionCulledMeshCount{0};
  uint32_t MeshSubmissionCount{0};
  uint32_t TriangleCount{0};
  glm::uvec2 DrawExtent{0u, 0u};
};

class RendererBackend {
public:
  virtual ~RendererBackend() = default;

  virtual void Init(const RendererCreateInfo &CreateInfo) = 0;
  virtual void Shutdown() = 0;
  virtual void BeginFrame() = 0;
  virtual std::shared_ptr<Mesh> CreateMesh(const MeshData &Mesh) = 0;
  virtual void RenderSceneMeshes(RenderScene &Scene) = 0;
  virtual void RenderFallbackBackground(RenderScene &Scene) = 0;
  virtual RendererFrameStats &AccessFrameStats() = 0;
  virtual const RendererFrameStats &GetFrameStats() const = 0;
  virtual void RenderImGui() = 0;
  virtual void EndFrame() = 0;
  virtual std::optional<CapturedFrame> ConsumeCapturedFrame() = 0;
};
} // namespace Axiom
