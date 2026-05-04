#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/RenderSurface.h"
#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanImGuiRenderer.h"
#include "Renderer/Vulkan/VulkanMaterialResources.h"
#include "Renderer/Vulkan/VulkanOcclusionCulling.h"
#include "Renderer/Vulkan/VulkanRendererTypes.h"
#include "Renderer/Vulkan/VulkanSceneRenderer.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

struct GLFWwindow;

namespace Axiom {
class VulkanRendererBackend final : public RendererBackend {
public:
  static VulkanRendererBackend &Get();
  static VulkanRendererBackend *TryGet();

  void Init(const RendererCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  std::shared_ptr<Mesh> CreateMesh(const MeshData &Mesh) override;
  void RenderSceneMeshes(RenderScene &Scene) override;
  void RenderFallbackBackground(RenderScene &Scene) override;
  RendererFrameStats &AccessFrameStats() override;
  const RendererFrameStats &GetFrameStats() const override;
  void RenderImGui() override;
  void EndFrame() override;
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) override;
  std::optional<CapturedFrame> ConsumeCapturedFrame() override;

  void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&Function);
  void EnqueueDeferredDestroy(std::function<void()> &&Function);
  bool IsInitialized() const { return m_IsInitialized; }

private:
  void InitSwapchain();
  void InitDescriptors();
  void InitTextureResources();
  void InitPipelines();
  void InitBackgroundPipelines();
  void InitMeshPipelines();
  void InitMeshFrameResources();

  void InitHzbResources();
  void CollectFrameStats(MeshFrameResources &Frame);
  void DrawBackground(VkCommandBuffer CommandBuffer);
  void DrawMeshes(VkCommandBuffer CommandBuffer, RenderScene &Scene);
  void BuildHzb(VkCommandBuffer CommandBuffer, MeshFrameResources &Frame);
  void RecordOffscreenCapture(VkCommandBuffer CommandBuffer);
  void ClearDepthImage(VkCommandBuffer CommandBuffer);
  void InitViewportReadbackBuffers();
  void PublishOffscreenFrame(FrameData &Frame);
  void Draw();
  static float HalfToFloat(uint16_t Value);
  static uint8_t LinearToByte(float Value);
  void ConvertCapturedFrameToRgba8(const AllocatedBuffer &ReadbackBuffer);

  FrameData &GetCurrentFrame() {
    return m_CommandContext.GetFrame(m_FrameNumber);
  }

  MeshFrameResources &GetCurrentMeshFrame() {
    return m_MeshFrames[m_FrameNumber % FRAME_OVERLAP];
  }

private:
  bool m_IsInitialized{false};
  uint64_t m_FrameNumber{0};
  bool m_StopRendering{false};
  bool m_RenderFallbackBackground{false};
  VkExtent2D m_WindowExtent{1700, 900};
  bool m_HasPresentationSurface{false};
  bool m_EnableImGui{true};

  GLFWwindow *m_Window{nullptr};
  RenderSurfacePtr m_Surface;
  IViewportFrameOutput *m_FrameOutput{nullptr};

  VulkanContext m_Context;
  VulkanDevice m_Device;
  VulkanSwapchain m_Swapchain;
  VulkanCommandContext m_CommandContext;

  DescriptorAllocator m_GlobalDescriptorAllocator;
  VkDescriptorSet m_DrawImageDescriptorSet{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_DrawImageDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_HzbReduceDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshGraphicsFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshComputeFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshDescriptorLayout{VK_NULL_HANDLE};
  VkSampler m_LinearDepthSampler{VK_NULL_HANDLE};
  VkSampler m_TextureSampler{VK_NULL_HANDLE};

  VkPipeline m_GradientPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_GradientPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_HzbReducePipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_HzbReducePipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshProjectPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshProjectPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshGraphicsPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshGraphicsPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshWireframePipeline{VK_NULL_HANDLE};
  VkPipeline m_MeshDepthPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshDepthPipelineLayout{VK_NULL_HANDLE};

  DeletionQueue m_MainDeletionQueue;

  AllocatedImage m_DrawImage;
  AllocatedImage m_DepthImage;
  AllocatedImage m_RasterDepthImage;
  AllocatedImage m_HzbImage;
  AllocatedBuffer m_CaptureReadbackBuffer;
  VkExtent2D m_DrawExtent{};
  std::vector<VkImageView> m_HzbMipImageViews;
  std::vector<VkDescriptorSet> m_HzbReduceDescriptorSets;
  std::vector<VkExtent2D> m_HzbMipExtents;
  std::vector<VkDeviceSize> m_HzbMipOffsets;
  VkDeviceSize m_HzbReadbackBufferSize{0};
  VkImageLayout m_HzbImageLayout{VK_IMAGE_LAYOUT_UNDEFINED};

  std::array<MeshFrameResources, FRAME_OVERLAP> m_MeshFrames{};
  VulkanImGuiRenderer m_ImGuiRenderer;
  VulkanMaterialResources m_MaterialResources;
  VulkanOcclusionCulling m_OcclusionCulling;
  VulkanSceneRenderer m_SceneRenderer;
  RenderScene *m_ActiveScene{nullptr};
  RendererFrameStats m_FrameStats{};
  float m_TimestampPeriod{0.0f};
  float m_RenderScale{0.5f};
  bool m_HasWarnedMeshSubmissionOverflow{false};
  RendererViewMode m_ViewMode{RendererViewMode::Lit};
  std::optional<CapturedFrame> m_CapturedFrame;
};
} // namespace Axiom
