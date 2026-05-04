#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanTypes.h"

namespace Axiom {
class VulkanImGuiRenderer {
public:
  struct InitInfo {
    void *WindowHandle{nullptr};
    VkInstance Instance{VK_NULL_HANDLE};
    VkPhysicalDevice PhysicalDevice{VK_NULL_HANDLE};
    VkDevice Device{VK_NULL_HANDLE};
    VkQueue Queue{VK_NULL_HANDLE};
    uint32_t QueueFamily{0};
    VkFormat SwapchainImageFormat{VK_FORMAT_UNDEFINED};
    DeletionQueue *DeletionQueue{nullptr};
  };

  void Init(const InitInfo &InitInfo);
  void BeginFrame() const;
  void BuildStatsUiAndRender(RendererFrameStats &FrameStats,
                             RendererViewMode &ViewMode) const;
  void RecordDrawData(VkCommandBuffer CommandBuffer, VkExtent2D Extent,
                      VkImageView TargetImageView) const;

  [[nodiscard]] bool IsEnabled() const { return m_IsEnabled; }

private:
  bool m_IsEnabled{false};
};
} // namespace Axiom
