#include "AxiomRHI/Vulkan/VulkanRhiObjects.h"

#include <cassert>
#include <vector>

namespace Axiom {
VulkanCommandList::VulkanCommandList(VkDevice Device, VkCommandPool CommandPool,
                                     VkCommandBuffer CommandBuffer,
                                     RHIQueueType QueueType,
                                     bool OwnsCommandBuffer)
    : m_Device(Device), m_CommandPool(CommandPool), m_CommandBuffer(CommandBuffer),
      m_QueueType(QueueType), m_OwnsCommandBuffer(OwnsCommandBuffer) {}

VulkanCommandList::~VulkanCommandList() {
  if (m_OwnsCommandBuffer && m_Device != VK_NULL_HANDLE &&
      m_CommandPool != VK_NULL_HANDLE && m_CommandBuffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &m_CommandBuffer);
  }
}

void VulkanCommandList::Begin() {
  VK_CHECK(vkResetCommandBuffer(m_CommandBuffer, 0));
  const VkCommandBufferBeginInfo BeginInfo =
      VkInit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &BeginInfo));
  m_IsRecording = true;
}

void VulkanCommandList::End() {
  VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));
  m_IsRecording = false;
}

VulkanFence::~VulkanFence() {
  if (m_OwnsFence && m_Device != VK_NULL_HANDLE && m_Fence != VK_NULL_HANDLE) {
    vkDestroyFence(m_Device, m_Fence, VK_NULL_HANDLE);
  }
}

VulkanSemaphore::~VulkanSemaphore() {
  if (m_OwnsSemaphore && m_Device != VK_NULL_HANDLE &&
      m_Semaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(m_Device, m_Semaphore, VK_NULL_HANDLE);
  }
}

VulkanQueue::VulkanQueue(VkQueue Queue, RHIQueueType Type)
    : m_Queue(Queue), m_Type(Type) {}

void VulkanQueue::Submit(IRHICommandList &CommandList,
                         std::span<const RHIQueueWaitInfo> WaitSemaphores,
                         std::span<const RHIQueueSignalInfo> SignalSemaphores,
                         IRHIFence *Fence) {
  auto *VulkanCommand = dynamic_cast<VulkanCommandList *>(&CommandList);
  assert(VulkanCommand != nullptr &&
         "Vulkan queues require Vulkan command list submissions");
  if (VulkanCommand == nullptr) {
    return;
  }

  std::vector<VkSemaphoreSubmitInfo> WaitInfos;
  WaitInfos.reserve(WaitSemaphores.size());
  for (const RHIQueueWaitInfo &WaitInfo : WaitSemaphores) {
    auto *Semaphore = dynamic_cast<VulkanSemaphore *>(WaitInfo.Semaphore);
    assert(Semaphore != nullptr &&
           "Vulkan queues require Vulkan semaphore wait objects");
    if (Semaphore == nullptr) {
      continue;
    }
    WaitInfos.push_back({
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = Semaphore->GetSemaphore(),
        .value = WaitInfo.Value,
        .stageMask = ToVulkanStage(WaitInfo.Stage),
    });
  }

  std::vector<VkSemaphoreSubmitInfo> SignalInfos;
  SignalInfos.reserve(SignalSemaphores.size());
  for (const RHIQueueSignalInfo &SignalInfo : SignalSemaphores) {
    auto *Semaphore = dynamic_cast<VulkanSemaphore *>(SignalInfo.Semaphore);
    assert(Semaphore != nullptr &&
           "Vulkan queues require Vulkan semaphore signal objects");
    if (Semaphore == nullptr) {
      continue;
    }
    SignalInfos.push_back({
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = Semaphore->GetSemaphore(),
        .value = SignalInfo.Value,
        .stageMask = ToVulkanStage(SignalInfo.Stage),
    });
  }

  const VkCommandBufferSubmitInfo CommandInfo =
      VkInit::CommandBufferSubmitInfo(VulkanCommand->GetCommandBuffer());
  const auto *VulkanFenceObject =
      Fence != nullptr ? dynamic_cast<VulkanFence *>(Fence) : nullptr;
  const VkFence VulkanFenceHandle =
      VulkanFenceObject != nullptr ? VulkanFenceObject->GetFence()
                                   : VK_NULL_HANDLE;

  const VkSubmitInfo2 SubmitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .waitSemaphoreInfoCount = static_cast<uint32_t>(WaitInfos.size()),
      .pWaitSemaphoreInfos =
          WaitInfos.empty() ? VK_NULL_HANDLE : WaitInfos.data(),
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = &CommandInfo,
      .signalSemaphoreInfoCount = static_cast<uint32_t>(SignalInfos.size()),
      .pSignalSemaphoreInfos =
          SignalInfos.empty() ? VK_NULL_HANDLE : SignalInfos.data(),
  };
  VK_CHECK(vkQueueSubmit2(m_Queue, 1, &SubmitInfo, VulkanFenceHandle));
}

VkPipelineStageFlags2 VulkanQueue::ToVulkanStage(RHICommandStage Stage) {
  switch (Stage) {
  case RHICommandStage::All:
    return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  case RHICommandStage::Draw:
    return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
           VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  case RHICommandStage::ColorAttachmentOutput:
    return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  case RHICommandStage::Compute:
    return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  case RHICommandStage::Transfer:
    return VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
  }

  return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
}
} // namespace Axiom
