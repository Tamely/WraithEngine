#pragma once

#include "AxiomRHI/Vulkan/VulkanDeletionQueue.h"
#include "AxiomRHI/Vulkan/VulkanTypes.h"

typedef struct VmaAllocator_T *VmaAllocator;

namespace Axiom {
class VulkanContext;

class VulkanDevice {
public:
  void Init(VulkanContext &Context);
  void Shutdown();

  VkPhysicalDevice PhysicalDevice{VK_NULL_HANDLE};
  VkDevice Device{VK_NULL_HANDLE};
  VkQueue GraphicsQueue{VK_NULL_HANDLE};
  uint32_t GraphicsQueueFamily{0};
  VkQueue TransferQueue{VK_NULL_HANDLE};
  uint32_t TransferQueueFamily{0};
  VmaAllocator Allocator{nullptr};

private:
  DeletionQueue m_DeletionQueue;
};
} // namespace Axiom
