#pragma once

#include "Renderer/Vulkan/VulkanTypes.h"

#include <cstddef>

namespace Axiom::VkBufferUtil {
AllocatedBuffer CreateBuffer(VmaAllocator Allocator, size_t Size,
                             VkBufferUsageFlags Usage,
                             VmaMemoryUsage MemoryUsage,
                             VmaAllocationCreateFlags Flags = 0);
void DestroyBuffer(VmaAllocator Allocator, AllocatedBuffer &Buffer);
AllocatedBuffer CreateBufferFromData(VmaAllocator Allocator, VkDevice Device,
                                     VkQueue Queue, VkCommandPool CommandPool,
                                     const void *Data, size_t Size,
                                     VkBufferUsageFlags Usage);
}
