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

void VulkanCommandList::BeginRendering(const void *RenderingInfo) {
  vkCmdBeginRendering(
      m_CommandBuffer, static_cast<const VkRenderingInfo *>(RenderingInfo));
}

void VulkanCommandList::EndRendering() { vkCmdEndRendering(m_CommandBuffer); }

void VulkanCommandList::SetViewport(const void *Viewport) {
  vkCmdSetViewport(m_CommandBuffer, 0, 1,
                   static_cast<const VkViewport *>(Viewport));
}

void VulkanCommandList::SetScissor(const void *Scissor) {
  vkCmdSetScissor(m_CommandBuffer, 0, 1, static_cast<const VkRect2D *>(Scissor));
}

void VulkanCommandList::BindPipeline(RHIBindPoint BindPoint,
                                     RHINativeHandle Pipeline) {
  vkCmdBindPipeline(m_CommandBuffer, ToVulkanBindPoint(BindPoint),
                    DecodeNativeHandle<VkPipeline>(Pipeline));
}

void VulkanCommandList::BindDescriptorSet(
    RHIBindPoint BindPoint, RHINativeHandle PipelineLayout, uint32_t FirstSet,
    std::span<const RHINativeHandle> Sets) {
  std::vector<VkDescriptorSet> DescriptorSets;
  DescriptorSets.reserve(Sets.size());
  for (RHINativeHandle Set : Sets) {
    DescriptorSets.push_back(DecodeNativeHandle<VkDescriptorSet>(Set));
  }

  vkCmdBindDescriptorSets(
      m_CommandBuffer, ToVulkanBindPoint(BindPoint),
      DecodeNativeHandle<VkPipelineLayout>(PipelineLayout), FirstSet,
      static_cast<uint32_t>(DescriptorSets.size()), DescriptorSets.data(), 0,
      VK_NULL_HANDLE);
}

void VulkanCommandList::PushConstants(RHINativeHandle PipelineLayout,
                                      uint32_t ShaderStages, uint32_t Offset,
                                      uint32_t Size, const void *Data) {
  vkCmdPushConstants(m_CommandBuffer,
                     DecodeNativeHandle<VkPipelineLayout>(PipelineLayout),
                     static_cast<VkShaderStageFlags>(ShaderStages), Offset, Size,
                     Data);
}

void VulkanCommandList::BindVertexBuffer(uint32_t FirstBinding,
                                         RHINativeHandle Buffer,
                                         uint64_t Offset) {
  const VkBuffer VulkanBuffer = DecodeNativeHandle<VkBuffer>(Buffer);
  const VkDeviceSize VulkanOffset = static_cast<VkDeviceSize>(Offset);
  vkCmdBindVertexBuffers(m_CommandBuffer, FirstBinding, 1, &VulkanBuffer,
                         &VulkanOffset);
}

void VulkanCommandList::BindIndexBuffer(RHINativeHandle Buffer, uint64_t Offset,
                                        RHIIndexType IndexType) {
  vkCmdBindIndexBuffer(m_CommandBuffer, DecodeNativeHandle<VkBuffer>(Buffer),
                       static_cast<VkDeviceSize>(Offset),
                       ToVulkanIndexType(IndexType));
}

void VulkanCommandList::DrawIndexed(uint32_t IndexCount, uint32_t InstanceCount,
                                    uint32_t FirstIndex, int32_t VertexOffset,
                                    uint32_t FirstInstance) {
  vkCmdDrawIndexed(m_CommandBuffer, IndexCount, InstanceCount, FirstIndex,
                   VertexOffset, FirstInstance);
}

void VulkanCommandList::DrawIndexedIndirect(RHINativeHandle Buffer,
                                            uint64_t Offset, uint32_t DrawCount,
                                            uint32_t Stride) {
  vkCmdDrawIndexedIndirect(m_CommandBuffer, DecodeNativeHandle<VkBuffer>(Buffer),
                           static_cast<VkDeviceSize>(Offset), DrawCount, Stride);
}

void VulkanCommandList::Dispatch(uint32_t GroupCountX, uint32_t GroupCountY,
                                 uint32_t GroupCountZ) {
  vkCmdDispatch(m_CommandBuffer, GroupCountX, GroupCountY, GroupCountZ);
}

void VulkanCommandList::CopyBuffer(RHINativeHandle SourceBuffer,
                                   RHINativeHandle DestinationBuffer,
                                   const void *CopyRegion) {
  vkCmdCopyBuffer(m_CommandBuffer, DecodeNativeHandle<VkBuffer>(SourceBuffer),
                  DecodeNativeHandle<VkBuffer>(DestinationBuffer), 1,
                  static_cast<const VkBufferCopy *>(CopyRegion));
}

void VulkanCommandList::CopyBufferToImage(RHINativeHandle SourceBuffer,
                                          RHINativeHandle DestinationImage,
                                          uint32_t DestinationImageLayout,
                                          const void *CopyRegion) {
  vkCmdCopyBufferToImage(
      m_CommandBuffer, DecodeNativeHandle<VkBuffer>(SourceBuffer),
      DecodeNativeHandle<VkImage>(DestinationImage),
      static_cast<VkImageLayout>(DestinationImageLayout), 1,
      static_cast<const VkBufferImageCopy *>(CopyRegion));
}

void VulkanCommandList::CopyImageToBuffer(RHINativeHandle SourceImage,
                                          uint32_t SourceImageLayout,
                                          RHINativeHandle DestinationBuffer,
                                          const void *CopyRegion) {
  vkCmdCopyImageToBuffer(
      m_CommandBuffer, DecodeNativeHandle<VkImage>(SourceImage),
      static_cast<VkImageLayout>(SourceImageLayout),
      DecodeNativeHandle<VkBuffer>(DestinationBuffer), 1,
      static_cast<const VkBufferImageCopy *>(CopyRegion));
}

void VulkanCommandList::PipelineBarrier(const void *DependencyInfo) {
  vkCmdPipelineBarrier2(
      m_CommandBuffer, static_cast<const VkDependencyInfo *>(DependencyInfo));
}

VkPipelineBindPoint
VulkanCommandList::ToVulkanBindPoint(RHIBindPoint BindPoint) {
  switch (BindPoint) {
  case RHIBindPoint::Graphics:
    return VK_PIPELINE_BIND_POINT_GRAPHICS;
  case RHIBindPoint::Compute:
    return VK_PIPELINE_BIND_POINT_COMPUTE;
  }

  return VK_PIPELINE_BIND_POINT_GRAPHICS;
}

VkIndexType VulkanCommandList::ToVulkanIndexType(RHIIndexType IndexType) {
  switch (IndexType) {
  case RHIIndexType::UInt16:
    return VK_INDEX_TYPE_UINT16;
  case RHIIndexType::UInt32:
    return VK_INDEX_TYPE_UINT32;
  }

  return VK_INDEX_TYPE_UINT32;
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
