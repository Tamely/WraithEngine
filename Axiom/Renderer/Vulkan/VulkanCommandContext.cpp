#include "Renderer/Vulkan/VulkanCommandContext.h"

#include "Renderer/Vulkan/VulkanInitializers.h"

namespace Axiom {
void VulkanCommandContext::Init(VkDevice Device, uint32_t GraphicsQueueFamily) {
  VkCommandPoolCreateInfo CommandPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = VK_NULL_HANDLE};
  CommandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  CommandPoolInfo.queueFamilyIndex = GraphicsQueueFamily;

  for (int i = 0; i < FRAME_OVERLAP; ++i) {
    VK_CHECK(vkCreateCommandPool(Device, &CommandPoolInfo, VK_NULL_HANDLE,
                                 &m_Frames[i].CommandPool));

    VkCommandBufferAllocateInfo CommandAllocateInfo =
        VkInit::CommandBufferAllocateInfo(m_Frames[i].CommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(Device, &CommandAllocateInfo,
                                      &m_Frames[i].MainCommandBuffer));
  }

  VK_CHECK(vkCreateCommandPool(Device, &CommandPoolInfo, VK_NULL_HANDLE,
                               &m_ImmCommandPool));

  VkCommandBufferAllocateInfo CommandAllocateInfo =
      VkInit::CommandBufferAllocateInfo(m_ImmCommandPool, 1);

  VK_CHECK(vkAllocateCommandBuffers(Device, &CommandAllocateInfo,
                                    &m_ImmCommandBuffer));

  VkFenceCreateInfo FenceCreateInfo =
      VkInit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo SemaphoreCreateInfo = VkInit::SemaphoreCreateInfo();

  for (int i = 0; i < FRAME_OVERLAP; ++i) {
    VK_CHECK(vkCreateFence(Device, &FenceCreateInfo, VK_NULL_HANDLE,
                           &m_Frames[i].RenderFence));

    VK_CHECK(vkCreateSemaphore(Device, &SemaphoreCreateInfo, VK_NULL_HANDLE,
                               &m_Frames[i].SwapchainSemaphore));
    VK_CHECK(vkCreateSemaphore(Device, &SemaphoreCreateInfo, VK_NULL_HANDLE,
                               &m_Frames[i].RenderSemaphore));
  }

  VK_CHECK(vkCreateFence(Device, &FenceCreateInfo, VK_NULL_HANDLE, &m_ImmFence));
}

void VulkanCommandContext::Shutdown(VkDevice Device) {
  for (int i = 0; i < FRAME_OVERLAP; ++i) {
    vkDestroyCommandPool(Device, m_Frames[i].CommandPool, VK_NULL_HANDLE);
    vkDestroyFence(Device, m_Frames[i].RenderFence, VK_NULL_HANDLE);
    vkDestroySemaphore(Device, m_Frames[i].RenderSemaphore, VK_NULL_HANDLE);
    vkDestroySemaphore(Device, m_Frames[i].SwapchainSemaphore, VK_NULL_HANDLE);
  }

  vkDestroyCommandPool(Device, m_ImmCommandPool, VK_NULL_HANDLE);
  vkDestroyFence(Device, m_ImmFence, VK_NULL_HANDLE);
}

FrameData &VulkanCommandContext::PrepareFrame(VkDevice Device,
                                              uint64_t FrameNumber) {
  auto &Frame = GetFrame(FrameNumber);
  VK_CHECK(vkWaitForFences(Device, 1, &Frame.RenderFence, VK_TRUE, 1000000000));
  VK_CHECK(vkResetFences(Device, 1, &Frame.RenderFence));
  Frame.DeletionQueue.Flush();
  return Frame;
}

void VulkanCommandContext::ImmediateSubmit(
    VkDevice Device, VkQueue Queue,
    std::function<void(VkCommandBuffer Command)> &&Function) {
  VK_CHECK(vkResetFences(Device, 1, &m_ImmFence));
  VK_CHECK(vkResetCommandBuffer(m_ImmCommandBuffer, 0));

  VkCommandBuffer Command = m_ImmCommandBuffer;

  VkCommandBufferBeginInfo CommandBeginInfo = VkInit::CommandBufferBeginInfo(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(Command, &CommandBeginInfo));

  Function(Command);

  VK_CHECK(vkEndCommandBuffer(Command));

  VkCommandBufferSubmitInfo CommandInfo =
      VkInit::CommandBufferSubmitInfo(Command);
  VkSubmitInfo2 Submit =
      VkInit::SubmitInfo(&CommandInfo, VK_NULL_HANDLE, VK_NULL_HANDLE);

  VK_CHECK(vkQueueSubmit2(Queue, 1, &Submit, m_ImmFence));
  VK_CHECK(vkWaitForFences(Device, 1, &m_ImmFence, true, 9999999999));
}
} // namespace Axiom

