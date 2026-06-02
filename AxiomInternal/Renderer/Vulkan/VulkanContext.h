#pragma once

#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <VkBootstrap.h>

namespace Axiom {
class IRenderSurface;

class VulkanContext {
public:
  void Init(const IRenderSurface &Surface);
  void Shutdown();

  VkInstance Instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT DebugMessenger{VK_NULL_HANDLE};
  VkSurfaceKHR Surface{VK_NULL_HANDLE};
  vkb::Instance BootstrapInstance{};

private:
  DeletionQueue m_DeletionQueue;
};
} // namespace Axiom
