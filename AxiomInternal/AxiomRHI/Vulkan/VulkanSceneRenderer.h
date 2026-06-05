#pragma once

#include "AxiomRHI/SceneRendererBackendFactory.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"

#include <optional>
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
  void RecordPreparedScenePasses(VkCommandBuffer CommandBuffer, RenderScene &Scene,
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
  void RecordBackground();
  void RecordDepthPrepass();
  void BuildHzb();
  void RecordComputeMeshPath();
  void RecordOpaqueForward();
  void RecordTranslucentForward();
  void FinalizeSceneFrame();
  void RenderFallbackBackground(RenderScene &Scene);
  void DrawBackgroundPass(VkCommandBuffer CommandBuffer, RenderScene *Scene);
  void BuildHzbPass(VkCommandBuffer CommandBuffer, MeshFrameResources &Frame);
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
  void RecordDepthPrepassPass(VkCommandBuffer CommandBuffer,
                              const MeshFrameResources &Frame) const;
  void RecordComputeMeshPathPass(VkCommandBuffer CommandBuffer,
                                 const MeshFrameResources &Frame) const;
  void RecordOpaqueForwardPass(VkCommandBuffer CommandBuffer,
                               const MeshFrameResources &Frame);
  void RecordTranslucentForwardPass(VkCommandBuffer CommandBuffer,
                                    const MeshFrameResources &Frame);
  void EnsureDrawImageLayout(VkCommandBuffer CommandBuffer,
                             VkImageLayout DesiredLayout);
  void EnsureRasterDepthLayout(VkCommandBuffer CommandBuffer,
                               VkImageLayout DesiredLayout);
  void BindMeshBuffers(VkCommandBuffer CommandBuffer, const VulkanMesh &Mesh) const;
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
  std::vector<ScenePassPrimitive> m_QueuedScenePasses;
  VkImageLayout m_SceneDrawImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout m_SceneRasterDepthLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};
} // namespace Axiom
