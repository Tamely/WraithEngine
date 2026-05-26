#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/RenderSurface.h"
#include "Renderer/Vulkan/GPUResourceQueue.h"
#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanDrawSubmissionSystem.h"
#include "Renderer/Vulkan/VulkanMaterialResources.h"
#include "Renderer/Vulkan/VulkanOcclusionCulling.h"
#include "Renderer/Vulkan/VulkanPipelineLibrary.h"
#include "Renderer/Vulkan/VulkanResourceManager.h"
#include "Renderer/Vulkan/VulkanRendererTypes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>

namespace Axiom {
class VulkanMesh;

class VulkanRendererBackend final : public RendererBackend {
public:
  void Init(const RendererCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options = {}) override;
  void PrepareSceneFrame(RenderScene &Scene) override;
  const VisibleSubmissionList &GetVisibleSubmissions() const override;
  void RecordDepthPrepass() override;
  void BuildHzb() override;
  void RecordOpaqueForward() override;
  void RecordTranslucentForward() override;
  void RecordComputeMeshPath() override;
  void RecordBackground() override;
  void FinalizeSceneFrame() override;
  void RenderFallbackBackground(RenderScene &Scene) override;
  RendererFrameStats &AccessFrameStats() override;
  const RendererFrameStats &GetFrameStats() const override;
  void RenderImGui() override;
  void EndFrame() override;
  void SetViewMode(RendererViewMode ViewMode) override;
  void SetViewportFrameUser(SessionUserId User) override;
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) override;
  std::optional<CapturedFrame> ConsumeCapturedFrame() override;
  bool IsInitialized() const { return m_IsInitialized; }
  VulkanMesh *ResolveMeshHandle(MeshHandle Handle) const;
  void RecordPreparedScenePasses(VkCommandBuffer CommandBuffer, RenderScene &Scene,
                                 uint64_t FrameNumber,
                                 RendererViewMode ViewMode);
  void DrawBackgroundPass(VkCommandBuffer CommandBuffer, RenderScene *Scene);
  void BuildHzbPass(VkCommandBuffer CommandBuffer, MeshFrameResources &Frame);

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

  MeshHandle AllocateMeshHandle();
  void Draw();
  void QueueScenePass(ScenePassPrimitive Pass);
  void ResetPreparedSceneState();
  glm::vec3 ComputeWorldCenter(const RenderMeshSubmission &Submission,
                               const VulkanMesh &Mesh) const;
  CameraFrameUniform BuildCameraData(const RenderScene &Scene,
                                     RendererViewMode ViewMode) const;
  void UpdateComputeFrameDescriptors(const MeshFrameResources &Frame) const;
  void UpdateDepthFrameDescriptors(const MeshFrameResources &Frame) const;
  void UpdateGraphicsFrameDescriptors(
      VkDescriptorSet GraphicsDescriptorSet, VkImageView TextureView,
      const VkDescriptorBufferInfo &CameraBufferInfo) const;
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
  bool m_IsInitialized{false};
  uint64_t m_FrameNumber{0};
  bool m_StopRendering{false};
  bool m_RenderFallbackBackground{false};
  VkExtent2D m_WindowExtent{1700, 900};
  bool m_HasPresentationSurface{false};
  bool m_EnableImGui{true};

  RenderSurfacePtr m_Surface;
  IViewportFrameOutput *m_FrameOutput{nullptr};

  VulkanContext m_Context;
  VulkanDevice m_Device;
  VulkanCommandContext m_CommandContext;
  VulkanResourceManager m_ResourceManager;
  VulkanPipelineLibrary m_PipelineLibrary;
  VulkanDrawSubmissionSystem m_DrawSubmissionSystem;
  VulkanMaterialResources m_MaterialResources;
  VulkanOcclusionCulling m_OcclusionCulling;
  std::shared_ptr<GPUResourceQueue> m_GpuResourceQueue;
  std::unordered_map<MeshHandle, std::weak_ptr<VulkanMesh>, MeshHandleHash>
      m_MeshesByHandle;
  uint64_t m_NextMeshHandleValue{1};
  RenderScene *m_ActiveScene{nullptr};
  RendererViewMode m_ViewMode{RendererViewMode::Lit};
  SessionUserId m_ViewportFrameUser{};
  PreparedSceneState m_PreparedSceneState{};
  std::vector<CandidateSubmission> m_CandidateScratch;
  std::vector<ScenePassPrimitive> m_QueuedScenePasses;
  bool m_HasWarnedMeshSubmissionOverflow{false};
  VkImageLayout m_SceneDrawImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout m_SceneRasterDepthLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};
} // namespace Axiom
