#include "Renderer/Vulkan/VulkanBuffer.h"

#include "Renderer/Vulkan/VulkanInitializers.h"

#include <cstring>

namespace Axiom::VkBufferUtil {
AllocatedBuffer CreateBuffer(VmaAllocator Allocator, size_t Size,
                             VkBufferUsageFlags Usage,
                             VmaMemoryUsage MemoryUsage,
                             VmaAllocationCreateFlags Flags) {
  VkBufferCreateInfo BufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .size = Size,
      .usage = Usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

  VmaAllocationCreateInfo AllocInfo{};
  AllocInfo.usage = MemoryUsage;
  AllocInfo.flags = Flags;

  AllocatedBuffer Buffer{};
  Buffer.Size = Size;
  VK_CHECK(vmaCreateBuffer(Allocator, &BufferInfo, &AllocInfo, &Buffer.Buffer,
                           &Buffer.Allocation, &Buffer.Info));
  return Buffer;
}

void DestroyBuffer(VmaAllocator Allocator, AllocatedBuffer &Buffer) {
  if (Buffer.Buffer == VK_NULL_HANDLE) {
    return;
  }

  vmaDestroyBuffer(Allocator, Buffer.Buffer, Buffer.Allocation);
  Buffer = {};
}

AllocatedBuffer CreateBufferFromData(VmaAllocator Allocator, VkDevice Device,
                                     VkQueue Queue, VkCommandPool CommandPool,
                                     const void *Data, size_t Size,
                                     VkBufferUsageFlags Usage) {
  auto StagingBuffer =
      CreateBuffer(Allocator, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VMA_MEMORY_USAGE_CPU_ONLY,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                       VMA_ALLOCATION_CREATE_MAPPED_BIT);
  std::memcpy(StagingBuffer.Info.pMappedData, Data, Size);

  auto GpuBuffer = CreateBuffer(Allocator, Size,
                                Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VMA_MEMORY_USAGE_GPU_ONLY);

  VkCommandBufferAllocateInfo AllocateInfo =
      VkInit::CommandBufferAllocateInfo(CommandPool, 1);
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
  VK_CHECK(vkAllocateCommandBuffers(Device, &AllocateInfo, &CommandBuffer));

  VkCommandBufferBeginInfo BeginInfo =
      VkInit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(CommandBuffer, &BeginInfo));

  VkBufferCopy CopyRegion{.srcOffset = 0, .dstOffset = 0, .size = Size};
  vkCmdCopyBuffer(CommandBuffer, StagingBuffer.Buffer, GpuBuffer.Buffer, 1,
                  &CopyRegion);

  VK_CHECK(vkEndCommandBuffer(CommandBuffer));

  VkCommandBufferSubmitInfo CommandInfo =
      VkInit::CommandBufferSubmitInfo(CommandBuffer);
  VkSubmitInfo2 SubmitInfo =
      VkInit::SubmitInfo(&CommandInfo, VK_NULL_HANDLE, VK_NULL_HANDLE);

  VkFenceCreateInfo FenceInfo = VkInit::FenceCreateInfo();
  VkFence Fence = VK_NULL_HANDLE;
  VK_CHECK(vkCreateFence(Device, &FenceInfo, VK_NULL_HANDLE, &Fence));
  VK_CHECK(vkQueueSubmit2(Queue, 1, &SubmitInfo, Fence));
  VK_CHECK(vkWaitForFences(Device, 1, &Fence, VK_TRUE, 9999999999));

  vkDestroyFence(Device, Fence, VK_NULL_HANDLE);
  vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
  DestroyBuffer(Allocator, StagingBuffer);

  return GpuBuffer;
}
} // namespace Axiom::VkBufferUtil
