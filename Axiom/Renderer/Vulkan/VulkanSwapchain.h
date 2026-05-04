#pragma once

#include "Renderer/Vulkan/VulkanTypes.h"

#include <vector>

namespace Axiom {
class VulkanContext;
class VulkanDevice;

class VulkanSwapchain {
public:
  void Init(VulkanContext &Context, VulkanDevice &Device, uint32_t Width,
            uint32_t Height);
  void Shutdown(VulkanDevice &Device);

  VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
  VkFormat ImageFormat{VK_FORMAT_UNDEFINED};
  std::vector<VkImage> Images;
  std::vector<VkImageView> ImageViews;
  VkExtent2D Extent{};
};
} // namespace Axiom

