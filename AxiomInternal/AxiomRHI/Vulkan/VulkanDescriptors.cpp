#include "AxiomRHI/Vulkan/VulkanDescriptors.h"

#include "Core/Log.h"

#include <algorithm>

#include <vulkan/vulkan_core.h>

void DescriptorLayoutBuilder::AddBinding(uint32_t Binding,
                                         VkDescriptorType Type) {
  VkDescriptorSetLayoutBinding NewBind{};
  NewBind.binding = Binding;
  NewBind.descriptorType = Type;
  NewBind.descriptorCount = 1;

  Bindings.push_back(NewBind);
}

void DescriptorLayoutBuilder::Clear() { Bindings.clear(); }

VkDescriptorSetLayout
DescriptorLayoutBuilder::Build(VkDevice Device, VkShaderStageFlags StageFlags,
                               void *pNext,
                               VkDescriptorSetLayoutCreateFlags Flags) {
  for (auto &Binding : Bindings) {
    Binding.stageFlags |= StageFlags;
  }

  VkDescriptorSetLayoutCreateInfo Info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  Info.pNext = pNext;

  Info.pBindings = Bindings.data();
  Info.bindingCount = static_cast<uint32_t>(Bindings.size());

  Info.flags = Flags;

  VkDescriptorSetLayout Set;
  VK_CHECK(vkCreateDescriptorSetLayout(Device, &Info, VK_NULL_HANDLE, &Set));

  return Set;
}

VkDescriptorPool DescriptorAllocator::CreatePool(VkDevice Device,
                                                 uint32_t MaxSets) {
  std::vector<VkDescriptorPoolSize> PoolSizes;
  PoolSizes.reserve(m_PoolRatios.size());
  for (PoolSizeRatio Ratio : m_PoolRatios) {
    PoolSizes.push_back(VkDescriptorPoolSize{
        .type = Ratio.Type,
        .descriptorCount =
            std::max(1u, static_cast<uint32_t>(Ratio.Ratio * MaxSets))});
  }

  VkDescriptorPoolCreateInfo PoolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  PoolInfo.maxSets = MaxSets;

  PoolInfo.pPoolSizes = PoolSizes.data();
  PoolInfo.poolSizeCount = static_cast<uint32_t>(PoolSizes.size());

  PoolInfo.flags = 0;

  VkDescriptorPool Pool = VK_NULL_HANDLE;
  VK_CHECK(vkCreateDescriptorPool(Device, &PoolInfo, VK_NULL_HANDLE, &Pool));
  return Pool;
}

void DescriptorAllocator::InitPool(VkDevice Device, uint32_t MaxSets,
                                   std::span<PoolSizeRatio> PoolRatios) {
  m_PoolRatios.assign(PoolRatios.begin(), PoolRatios.end());
  m_Pools.clear();
  m_NextPoolMaxSets = std::max(1u, MaxSets);
  m_Pools.push_back(CreatePool(Device, m_NextPoolMaxSets));
}

void DescriptorAllocator::ClearDescriptors(VkDevice Device) {
  for (VkDescriptorPool Pool : m_Pools) {
    vkResetDescriptorPool(Device, Pool, 0);
  }
}

void DescriptorAllocator::DestroyPool(VkDevice Device) {
  for (VkDescriptorPool Pool : m_Pools) {
    vkDestroyDescriptorPool(Device, Pool, VK_NULL_HANDLE);
  }
  m_Pools.clear();
  m_PoolRatios.clear();
  m_NextPoolMaxSets = 0;
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice Device,
                                              VkDescriptorSetLayout Layout) {
  VkDescriptorSetAllocateInfo AllocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .descriptorPool = VK_NULL_HANDLE,
      .descriptorSetCount = 1,
      .pSetLayouts = &Layout};

  for (;;) {
    for (VkDescriptorPool Pool : m_Pools) {
      AllocInfo.descriptorPool = Pool;
      VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
      const VkResult Result =
          vkAllocateDescriptorSets(Device, &AllocInfo, &DescriptorSet);
      if (Result == VK_SUCCESS) {
        return DescriptorSet;
      }
      if (Result != VK_ERROR_OUT_OF_POOL_MEMORY &&
          Result != VK_ERROR_FRAGMENTED_POOL) {
        A_CORE_ERROR("Detected Vulkan error: {0}", VkResultToString(Result));
        Axiom::Log::Flush();
        abort();
      }
    }

    const uint32_t NewPoolMaxSets = std::max(64u, m_NextPoolMaxSets);
    A_CORE_WARN(
        "Descriptor pool exhausted; allocating an additional pool with capacity for {0} descriptor sets.",
        NewPoolMaxSets);
    m_Pools.push_back(CreatePool(Device, NewPoolMaxSets));
    m_NextPoolMaxSets = std::max(NewPoolMaxSets + 1, NewPoolMaxSets * 2);
  }
}
