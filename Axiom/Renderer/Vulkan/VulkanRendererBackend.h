#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <functional>

struct GLFWwindow;

namespace Axiom {
struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

class VulkanRendererBackend final : public RendererBackend {
public:
  static VulkanRendererBackend &Get();

  void Init(const RendererCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  void RenderFallbackBackground(RenderScene &Scene) override;
  void RenderImGui() override;
  void EndFrame() override;

  void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)> &&Function);

private:
  void InitSwapchain();
  void InitDescriptors();
  void InitPipelines();
  void InitBackgroundPipelines();
  void InitImGui();

  void DrawBackground(VkCommandBuffer CommandBuffer);
  void DrawImGui(VkCommandBuffer Command, VkImageView TargetImageView);
  void Draw();

  FrameData &GetCurrentFrame() {
    return m_CommandContext.GetFrame(m_FrameNumber);
  }

private:
  bool m_IsInitialized{false};
  int m_FrameNumber{0};
  bool m_StopRendering{false};
  bool m_RenderFallbackBackground{false};
  VkExtent2D m_WindowExtent{1700, 900};

  GLFWwindow *m_Window{nullptr};

  VulkanContext m_Context;
  VulkanDevice m_Device;
  VulkanSwapchain m_Swapchain;
  VulkanCommandContext m_CommandContext;

  DescriptorAllocator m_GlobalDescriptorAllocator;
  VkDescriptorSet m_DrawImageDescriptors;
  VkDescriptorSetLayout m_DrawImageDescriptorLayout;

  VkPipeline m_GradientPipeline;
  VkPipelineLayout m_GradientPipelineLayout;

  DeletionQueue m_MainDeletionQueue;

  AllocatedImage m_DrawImage;
  VkExtent2D m_DrawExtent;
};
} // namespace Axiom

