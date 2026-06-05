#pragma once

#include "RHI/IRHI.h"
#include "AxiomRHI/Vulkan/VulkanInitializers.h"
#include "AxiomRHI/Vulkan/VulkanTypes.h"

#include <memory>

namespace Axiom {
class VulkanCommandList final : public IRHICommandList {
public:
  VulkanCommandList(VkDevice Device, VkCommandPool CommandPool,
                    VkCommandBuffer CommandBuffer, RHIQueueType QueueType,
                    bool OwnsCommandBuffer);
  ~VulkanCommandList() override;

  void Begin() override;
  void End() override;
  bool IsRecording() const override { return m_IsRecording; }
  RHIQueueType GetQueueType() const override { return m_QueueType; }

  VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

private:
  VkDevice m_Device{VK_NULL_HANDLE};
  VkCommandPool m_CommandPool{VK_NULL_HANDLE};
  VkCommandBuffer m_CommandBuffer{VK_NULL_HANDLE};
  RHIQueueType m_QueueType{RHIQueueType::Graphics};
  bool m_OwnsCommandBuffer{false};
  bool m_IsRecording{false};
};

class VulkanFence final : public IRHIFence {
public:
  VulkanFence(VkDevice Device, VkFence Fence, bool OwnsFence)
      : m_Device(Device), m_Fence(Fence), m_OwnsFence(OwnsFence) {}
  ~VulkanFence() override;

  VkFence GetFence() const { return m_Fence; }

private:
  VkDevice m_Device{VK_NULL_HANDLE};
  VkFence m_Fence{VK_NULL_HANDLE};
  bool m_OwnsFence{false};
};

class VulkanSemaphore final : public IRHISemaphore {
public:
  VulkanSemaphore(VkDevice Device, VkSemaphore Semaphore, bool Timeline,
                  bool OwnsSemaphore)
      : m_Device(Device), m_Semaphore(Semaphore), m_IsTimeline(Timeline),
        m_OwnsSemaphore(OwnsSemaphore) {}
  ~VulkanSemaphore() override;

  bool IsTimeline() const override { return m_IsTimeline; }
  VkSemaphore GetSemaphore() const { return m_Semaphore; }

private:
  VkDevice m_Device{VK_NULL_HANDLE};
  VkSemaphore m_Semaphore{VK_NULL_HANDLE};
  bool m_IsTimeline{false};
  bool m_OwnsSemaphore{false};
};

class VulkanQueue final : public IRHIQueue {
public:
  VulkanQueue(VkQueue Queue, RHIQueueType Type);

  RHIQueueType GetType() const override { return m_Type; }
  void Submit(IRHICommandList &CommandList,
              std::span<const RHIQueueWaitInfo> WaitSemaphores = {},
              std::span<const RHIQueueSignalInfo> SignalSemaphores = {},
              IRHIFence *Fence = nullptr) override;

  VkQueue GetQueue() const { return m_Queue; }

private:
  static VkPipelineStageFlags2 ToVulkanStage(RHICommandStage Stage);

private:
  VkQueue m_Queue{VK_NULL_HANDLE};
  RHIQueueType m_Type{RHIQueueType::Graphics};
};

class VulkanBufferHandle final : public IRHIBuffer {
public:
  explicit VulkanBufferHandle(RHIBufferDesc Desc) : m_Desc(std::move(Desc)) {}

  const RHIBufferDesc &GetDesc() const override { return m_Desc; }

private:
  RHIBufferDesc m_Desc{};
};

class VulkanTextureHandle final : public IRHITexture {
public:
  explicit VulkanTextureHandle(RHITextureDesc Desc) : m_Desc(std::move(Desc)) {}

  const RHITextureDesc &GetDesc() const override { return m_Desc; }

private:
  RHITextureDesc m_Desc{};
};

class VulkanPipelineHandle final : public IRHIPipeline {
public:
  explicit VulkanPipelineHandle(RHIPipelineType Type) : m_Type(Type) {}

  RHIPipelineType GetType() const override { return m_Type; }

private:
  RHIPipelineType m_Type{RHIPipelineType::Graphics};
};

class VulkanDescriptorTableHandle final : public IRHIDescriptorTable {};

class VulkanBindGroupHandle final : public IRHIBindGroup {};

class VulkanSwapchainHandle final : public IRHISwapchain {};
} // namespace Axiom
