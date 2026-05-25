#pragma once

#include "Renderer/RenderScene.h"
#include "Renderer/RenderSurface.h"
#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanGizmoRenderer.h"
#include "Renderer/Vulkan/VulkanImGuiRenderer.h"
#include "Renderer/Vulkan/VulkanLightBillboardRenderer.h"
#include "Renderer/Vulkan/VulkanMaterialResources.h"
#include "Renderer/Vulkan/VulkanOcclusionCulling.h"
#include "Renderer/Vulkan/VulkanPipelineLibrary.h"
#include "Renderer/Vulkan/VulkanResourceManager.h"
#include "Renderer/Vulkan/VulkanSceneRenderer.h"

#include <functional>
#include <optional>
#include <vector>

namespace Axiom {
class VulkanContext;

class VulkanDrawSubmissionSystem {
public:
  struct CreateInfo {
    IRenderSurface *Surface{nullptr};
    VulkanContext &Context;
    VulkanDevice &Device;
    VulkanCommandContext &CommandContext;
    VulkanResourceManager &Resources;
    VulkanPipelineLibrary &Pipelines;
    VulkanMaterialResources &MaterialResources;
    VulkanOcclusionCulling &OcclusionCulling;
    bool EnableImGui{false};
    bool HasPresentationSurface{false};
    std::function<void()> DestroyResourceManagerHDRTexture;
  };

  struct FrameRequest {
    uint64_t FrameNumber{0};
    RenderScene *ActiveScene{nullptr};
    RendererViewMode ViewMode{RendererViewMode::Lit};
    SessionUserId ViewportFrameUser{};
    IViewportFrameOutput *FrameOutput{nullptr};
  };

  void Init(const CreateInfo &CreateInfo);
  void Shutdown();

  void BeginFrame(bool StopRendering);
  void RenderImGui(bool StopRendering, RendererViewMode ViewMode);
  void DrawFrame(const FrameRequest &Request);
  void SubmitTransferUpload(std::function<void(VkCommandBuffer)> &&RecordUpload,
                            std::function<void()> &&Cleanup);

  bool IsInitialized() const { return m_IsInitialized; }
  bool IsImGuiEnabled() const { return m_EnableImGui; }
  RendererFrameStats &AccessFrameStats() { return m_FrameStats; }
  const RendererFrameStats &GetFrameStats() const { return m_FrameStats; }
  std::optional<CapturedFrame> ConsumeCapturedFrame();

private:
  struct PendingTransfer {
    uint64_t SignalValue{0};
    VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
    std::function<void()> Cleanup;
  };

  void InitSpecializedRenderers();
  void InitTransferQueue();
  void ShutdownTransferQueue();
  void CollectCompletedTransfers();
  void CollectFrameStats(MeshFrameResources &Frame);
  void DrawBackground(VkCommandBuffer CommandBuffer, const FrameRequest &Request);
  void BuildHzb(VkCommandBuffer CommandBuffer, MeshFrameResources &Frame);
  void ClearDepthImage(VkCommandBuffer CommandBuffer, uint64_t FrameNumber);
  void DrawMeshes(VkCommandBuffer CommandBuffer, RenderScene &Scene,
                  uint64_t FrameNumber, RendererViewMode ViewMode);
  void RecordOffscreenCapture(VkCommandBuffer CommandBuffer,
                              const AllocatedBuffer &ReadbackBuffer,
                              VkExtent2D DrawExtent);
  void PublishCompletedOffscreenFrames(IViewportFrameOutput *FrameOutput);
  static float HalfToFloat(uint16_t Value);
  static uint8_t LinearToByte(float Value);
  void ConvertCapturedFrameToRgba8(const AllocatedBuffer &ReadbackBuffer,
                                   uint64_t FrameNumber, VkExtent2D DrawExtent);

private:
  IRenderSurface *m_Surface{nullptr};
  VulkanContext *m_Context{nullptr};
  VulkanDevice *m_Device{nullptr};
  VulkanCommandContext *m_CommandContext{nullptr};
  VulkanResourceManager *m_Resources{nullptr};
  VulkanPipelineLibrary *m_Pipelines{nullptr};
  VulkanMaterialResources *m_MaterialResources{nullptr};
  VulkanOcclusionCulling *m_OcclusionCulling{nullptr};
  bool m_EnableImGui{false};
  bool m_HasPresentationSurface{false};
  bool m_IsInitialized{false};

  VulkanGizmoRenderer m_GizmoRenderer;
  VulkanImGuiRenderer m_ImGuiRenderer;
  VulkanLightBillboardRenderer m_LightBillboardRenderer;
  VulkanSceneRenderer m_SceneRenderer;
  MaterialInstanceRef m_LightBillboardMaterial;

  RendererFrameStats m_FrameStats{};
  std::optional<CapturedFrame> m_CapturedFrame;
  float m_TimestampPeriod{0.0f};
  bool m_HasWarnedMeshSubmissionOverflow{false};

  VkQueue m_TransferQueue{VK_NULL_HANDLE};
  uint32_t m_TransferQueueFamily{0};
  VkCommandPool m_TransferCommandPool{VK_NULL_HANDLE};
  VkSemaphore m_TransferTimelineSemaphore{VK_NULL_HANDLE};
  uint64_t m_NextTransferSignalValue{0};
  uint64_t m_LastGraphicsWaitValue{0};
  std::vector<PendingTransfer> m_PendingTransfers;
  DeletionQueue m_RendererDeletionQueue;
};
} // namespace Axiom
