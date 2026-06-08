#pragma once

#include "AxiomRHI/SceneRendererBackendFactory.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"

#include <optional>
#include <span>
#include <vector>

namespace Axiom {
class VulkanRhiDevice;
class VulkanMesh;

class VulkanSceneRenderer final : public ISceneRendererBackend {
public:
  void Init(IRHIDevice &Device, const RendererCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options) override;
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material) override;
  void UpdateMaterialHandle(MaterialHandle Handle,
                            const MaterialInstance &Material) override;
  void Render(RenderScene &Scene) override;
  void RenderImGui() override;
  void EndFrame() override;
  void SetViewMode(RendererViewMode ViewMode) override;
  void SetViewportFrameUser(SessionUserId User) override;
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) override;
  std::optional<CapturedFrame> ConsumeCapturedFrame() override;
  RendererFrameStats &AccessFrameStats() override;
  const RendererFrameStats &GetFrameStats() const override;
  void RecordPreparedScenePasses(IRHICommandList &CommandList, RenderScene &Scene,
                                 uint64_t FrameNumber,
                                 RendererViewMode ViewMode);

private:
  enum class ScenePassPrimitive {
    Background,
    DepthPrepass,
    Hzb,
    ComputeMeshPath,
    OpaqueForward,
    TranslucentForward,
  };

  struct CandidateSubmission {
    uint32_t SubmissionIndex{0};
    MeshHandle MeshHandle{};
    VulkanMesh *Mesh{nullptr};
    float SortDepth{0.0f};

    constexpr bool operator==(const CandidateSubmission &) const = default;
  };

  struct SubmissionCullInput {
    VulkanMesh *Mesh{nullptr};
  };

  struct PreparedSceneState {
    RenderScene *Scene{nullptr};
    CameraFrameUniform CameraData{};
    VisibleSubmissionList VisibleSubmissions;
    bool ForceWireframe{false};
    bool HasPreparedCamera{false};
    bool HasQueuedFinalize{false};
  };

  void PrepareSceneFrame(RenderScene &Scene);
  size_t BuildCullCandidatesSerial(
      const RenderScene &Scene, std::span<const SubmissionCullInput> Inputs,
      std::vector<CandidateSubmission> &Candidates) const;
  size_t BuildCullCandidatesParallel(
      const RenderScene &Scene, std::span<const SubmissionCullInput> Inputs,
      std::vector<CandidateSubmission> &Candidates) const;
  bool ShouldUseParallelCull(size_t SubmissionCount) const;
  void RecordBackground();
  void RecordDepthPrepass();
  void BuildHzb();
  void RecordComputeMeshPath();
  void RecordOpaqueForward();
  void RecordTranslucentForward();
  void FinalizeSceneFrame();
  void RenderFallbackBackground(RenderScene &Scene);
  void DrawBackgroundPass(IRHICommandList &CommandList, RenderScene *Scene);
  void BuildHzbPass(IRHICommandList &CommandList, MeshFrameResources &Frame);
  void QueueScenePass(ScenePassPrimitive Pass);
  void ResetPreparedSceneState();
  glm::vec3 ComputeWorldCenter(const RenderMeshSubmission &Submission,
                               const VulkanMesh &Mesh) const;
  CameraFrameUniform BuildCameraData(const RenderScene &Scene,
                                     RendererViewMode ViewMode) const;
  void UpdateComputeFrameDescriptors(const MeshFrameResources &Frame) const;
  void UpdateDepthFrameDescriptors(const MeshFrameResources &Frame) const;
  void UpdateGraphicsFrameDescriptors(const MeshFrameResources &Frame) const;
  void PrepareGraphicsMaterialDescriptors();
  void RecordDepthPrepassPass(IRHICommandList &CommandList,
                              const MeshFrameResources &Frame) const;
  void RecordComputeMeshPathPass(IRHICommandList &CommandList,
                                 const MeshFrameResources &Frame) const;
  void RecordOpaqueForwardPass(IRHICommandList &CommandList,
                               const MeshFrameResources &Frame);
  void RecordTranslucentForwardPass(IRHICommandList &CommandList,
                                    const MeshFrameResources &Frame);
  void EnsureDrawImageLayout(IRHICommandList &CommandList,
                             VkImageLayout DesiredLayout);
  void EnsureRasterDepthLayout(IRHICommandList &CommandList,
                               VkImageLayout DesiredLayout);
  void BindMeshBuffers(IRHICommandList &CommandList, const VulkanMesh &Mesh) const;
  const RenderMeshSubmission &GetSubmission(uint32_t SubmissionIndex) const;
  VulkanMesh *ResolveVisibleMesh(const VisibleSubmission &Visible) const;
  VkExtent2D GetDrawExtent2D() const;

private:
  VulkanRhiDevice *m_Device{nullptr};
  bool m_StopRendering{false};
  bool m_RenderFallbackBackground{false};
  RenderScene *m_ActiveScene{nullptr};
  RendererViewMode m_ViewMode{RendererViewMode::Lit};
  SessionUserId m_ViewportFrameUser{};
  IViewportFrameOutput *m_FrameOutput{nullptr};
  PreparedSceneState m_PreparedSceneState{};
  std::vector<CandidateSubmission> m_CandidateScratch;
  std::vector<CandidateSubmission> m_VerificationCandidateScratch;
  std::vector<SubmissionCullInput> m_CullInputScratch;
  std::vector<ScenePassPrimitive> m_QueuedScenePasses;
  bool m_EnableParallelCull{AXIOM_PARALLEL_CULL != 0};
  bool m_VerifyParallelCull{AXIOM_VERIFY_PARALLEL_CULL != 0};
  VkImageLayout m_SceneDrawImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout m_SceneRasterDepthLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};
} // namespace Axiom
