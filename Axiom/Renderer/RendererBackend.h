#pragma once

#include "Core/RenderRuntime.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderScene.h"
#include "Renderer/RenderTechnique.h"

#include <glm/vec2.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Axiom {
class IRenderSurface;
struct MeshData;

enum class RendererBackendType { Vulkan };
enum class RendererTechniqueType : uint32_t {
  Forward = 0,
};
struct RendererCreateInfo {
  RenderSurfacePtr TargetSurface;
  IViewportFrameOutput *FrameOutput{nullptr};
  uint32_t Width{0};
  uint32_t Height{0};
  RendererBackendType BackendType{RendererBackendType::Vulkan};
  RendererTechniqueType Technique{RendererTechniqueType::Forward};
  RenderTechnique::AttachmentRequirements AttachmentRequirements{};
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
#if !defined(NDEBUG)
  uint32_t DebugGraphicsMaterialDescriptorUpdates{0};
  uint32_t DebugOpaqueMaterialDescriptorBinds{0};
  uint32_t DebugTranslucentMaterialDescriptorBinds{0};
  uint32_t DebugOpaqueUniqueMaterialCount{0};
  uint32_t DebugTranslucentUniqueMaterialCount{0};
#endif
};

class RendererBackend {
public:
  virtual ~RendererBackend() = default;

  virtual void Init(const RendererCreateInfo &CreateInfo) = 0;
  virtual void Shutdown() = 0;
  virtual void BeginFrame() = 0;
  virtual std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options = {}) = 0;
  virtual MaterialHandle CreateMaterialHandle(const MaterialInstance &Material) = 0;
  virtual void UpdateMaterialHandle(MaterialHandle Handle,
                                    const MaterialInstance &Material) = 0;
  virtual void PrepareSceneFrame(RenderScene &Scene) = 0;
  virtual const VisibleSubmissionList &GetVisibleSubmissions() const = 0;
  virtual void RecordDepthPrepass() = 0;
  virtual void BuildHzb() = 0;
  virtual void RecordOpaqueForward() = 0;
  virtual void RecordTranslucentForward() = 0;
  virtual void RecordComputeMeshPath() = 0;
  virtual void RecordBackground() = 0;
  virtual void FinalizeSceneFrame() = 0;
  virtual void RenderFallbackBackground(RenderScene &Scene) = 0;
  virtual RendererFrameStats &AccessFrameStats() = 0;
  virtual const RendererFrameStats &GetFrameStats() const = 0;
  virtual void RenderImGui() = 0;
  virtual void EndFrame() = 0;
  virtual void SetViewMode(RendererViewMode ViewMode) = 0;
  virtual void SetViewportFrameUser(SessionUserId User) = 0;
  virtual void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) = 0;
  virtual std::optional<CapturedFrame> ConsumeCapturedFrame() = 0;
};
} // namespace Axiom
