#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <array>
#include <functional>
#include <memory>

struct GLFWwindow;

namespace Axiom {
struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct CameraFrameUniform {
  glm::mat4 View{1.0f};
  glm::mat4 Projection{1.0f};
  glm::mat4 ViewProjection{1.0f};
  glm::vec4 CameraPosition{0.0f};
  glm::vec4 ViewportSize{0.0f};
};

struct MeshProjectPushConstants {
  glm::mat4 Model{1.0f};
  glm::uvec4 Counts{0u};
};

struct MeshRasterPushConstants {
  glm::uvec4 Counts{0u};
};

class VulkanMesh;

class VulkanRendererBackend final : public RendererBackend {
public:
  static VulkanRendererBackend &Get();

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

  void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&Function);

private:
  static constexpr uint32_t MaxMeshSubmissionsPerFrame = 64;
  static constexpr uint32_t TimestampQueryCount = 4;

struct MeshFrameResources {
  AllocatedBuffer CameraBuffer;
  VkDescriptorSet FrameDescriptorSet{VK_NULL_HANDLE};
  VkQueryPool TimestampQueryPool{VK_NULL_HANDLE};
  bool HasValidTimestamps{false};
  };

  void InitSwapchain();
  void InitDescriptors();
  void InitPipelines();
  void InitBackgroundPipelines();
  void InitMeshPipelines();
  void InitMeshFrameResources();
  void InitImGui();

  void CollectFrameStats(MeshFrameResources &Frame);
  void DrawStatsPanel();
  void DrawBackground(VkCommandBuffer CommandBuffer);
  void DrawMeshes(VkCommandBuffer CommandBuffer, RenderScene &Scene);
  void DrawImGui(VkCommandBuffer Command, VkImageView TargetImageView);
  void ClearDepthImage(VkCommandBuffer CommandBuffer);
  void Draw();

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

  GLFWwindow *m_Window{nullptr};

  VulkanContext m_Context;
  VulkanDevice m_Device;
  VulkanSwapchain m_Swapchain;
  VulkanCommandContext m_CommandContext;

  DescriptorAllocator m_GlobalDescriptorAllocator;
  VkDescriptorSet m_DrawImageDescriptorSet{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_DrawImageDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshDescriptorLayout{VK_NULL_HANDLE};

  VkPipeline m_GradientPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_GradientPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshProjectPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshProjectPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshPipelineLayout{VK_NULL_HANDLE};

  DeletionQueue m_MainDeletionQueue;

  AllocatedImage m_DrawImage;
  AllocatedImage m_DepthImage;
  VkExtent2D m_DrawExtent{};

  std::array<MeshFrameResources, FRAME_OVERLAP> m_MeshFrames{};
  RenderScene *m_ActiveScene{nullptr};
  RendererFrameStats m_FrameStats{};
  float m_TimestampPeriod{0.0f};
  float m_RenderScale{0.5f};
  bool m_HasWarnedMeshSubmissionOverflow{false};
};
} // namespace Axiom
