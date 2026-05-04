#pragma once

#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <VkBootstrap.h>

struct GLFWwindow;

namespace Axiom {
class VulkanContext {
public:
  void Init(GLFWwindow *Window = nullptr);
  void Shutdown();

  VkInstance Instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT DebugMessenger{VK_NULL_HANDLE};
  VkSurfaceKHR Surface{VK_NULL_HANDLE};
  vkb::Instance BootstrapInstance{};

private:
  DeletionQueue m_DeletionQueue;
};
} // namespace Axiom

