#pragma once

#include "Renderer/Vulkan/VulkanDeletionQueue.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <functional>

namespace Axiom {
struct FrameData {
  VkCommandPool CommandPool{VK_NULL_HANDLE};
  VkCommandBuffer MainCommandBuffer{VK_NULL_HANDLE};
  VkSemaphore SwapchainSemaphore{VK_NULL_HANDLE};
  VkSemaphore RenderSemaphore{VK_NULL_HANDLE};
  VkFence RenderFence{VK_NULL_HANDLE};
  DeletionQueue DeletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanCommandContext {
public:
  void Init(VkDevice Device, uint32_t GraphicsQueueFamily);
  void Shutdown(VkDevice Device);

  FrameData &GetFrame(uint64_t FrameNumber) {
    return m_Frames[FrameNumber % FRAME_OVERLAP];
  }

  void ImmediateSubmit(VkDevice Device, VkQueue Queue,
                       std::function<void(VkCommandBuffer Command)> &&Function);

private:
  FrameData m_Frames[FRAME_OVERLAP];
  VkFence m_ImmFence{VK_NULL_HANDLE};
  VkCommandBuffer m_ImmCommandBuffer{VK_NULL_HANDLE};
  VkCommandPool m_ImmCommandPool{VK_NULL_HANDLE};
};
} // namespace Axiom

