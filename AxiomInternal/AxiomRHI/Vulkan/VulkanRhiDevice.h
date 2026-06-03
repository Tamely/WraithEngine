#pragma once

#include "Renderer/RenderSurface.h"
#include "Renderer/RendererTypes.h"
#include "RHI/IRHI.h"
#include "AxiomRHI/Vulkan/GPUResourceQueue.h"
#include "AxiomRHI/Vulkan/VulkanCommandContext.h"
#include "AxiomRHI/Vulkan/VulkanContext.h"
#include "AxiomRHI/Vulkan/VulkanDevice.h"
#include "AxiomRHI/Vulkan/VulkanDrawSubmissionSystem.h"
#include "AxiomRHI/Vulkan/VulkanMaterialResources.h"
#include "AxiomRHI/Vulkan/VulkanOcclusionCulling.h"
#include "AxiomRHI/Vulkan/VulkanPipelineLibrary.h"
#include "AxiomRHI/Vulkan/VulkanResourceManager.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>

namespace Axiom {
class VulkanMesh;

class VulkanRhiDevice final : public IRHIDevice {
public:
  void Init(const RHIDeviceCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  void WaitIdle() override;
  IRHIQueue *GetQueue(RHIQueueType Type) override;
  std::unique_ptr<IRHICommandList> CreateCommandList(RHIQueueType Type) override;
  std::unique_ptr<IRHIBuffer> CreateBuffer(const RHIBufferDesc &Desc) override;
  std::unique_ptr<IRHITexture> CreateTexture(const RHITextureDesc &Desc) override;
  std::unique_ptr<IRHIPipeline> CreatePipeline(const RHIPipelineDesc &Desc) override;
  std::unique_ptr<IRHIDescriptorTable> CreateDescriptorTable() override;
  std::unique_ptr<IRHIBindGroup> CreateBindGroup() override;
  std::unique_ptr<IRHISwapchain> CreateSwapchain() override;
  std::unique_ptr<IRHIFence> CreateFence() override;
  std::unique_ptr<IRHISemaphore> CreateSemaphore(bool Timeline) override;

  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options = {});
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material);
  void UpdateMaterialHandle(MaterialHandle Handle,
                            const MaterialInstance &Material);
  void PrepareSceneFrame(RenderScene &Scene);
  const VisibleSubmissionList &GetVisibleSubmissions() const;
  void RecordDepthPrepass();
  void BuildHzb();
  void RecordOpaqueForward();
  void RecordTranslucentForward();
  void RecordComputeMeshPath();
  void RecordBackground();
  void FinalizeSceneFrame();
  void RenderFallbackBackground(RenderScene &Scene);
  RendererFrameStats &AccessFrameStats();
  const RendererFrameStats &GetFrameStats() const;
  void RenderImGui();
  void EndFrame();
  void SetViewMode(RendererViewMode ViewMode);
  void SetViewportFrameUser(SessionUserId User);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);
  std::optional<CapturedFrame> ConsumeCapturedFrame();
  bool IsInitialized() const { return m_IsInitialized; }
  VulkanMesh *ResolveMeshHandle(MeshHandle Handle) const;
  const MaterialInstance *ResolveMaterialHandle(MaterialHandle Handle) const;
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
  VkImageLayout m_SceneDrawImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout m_SceneRasterDepthLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};
} // namespace Axiom
