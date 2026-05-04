#include "Renderer/Vulkan/VulkanDescriptors.h"
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

void DescriptorAllocator::InitPool(VkDevice Device, uint32_t MaxSets,
                                   std::span<PoolSizeRatio> PoolRatios) {
  std::vector<VkDescriptorPoolSize> PoolSizes;
  for (PoolSizeRatio Ratio : PoolRatios) {
    PoolSizes.push_back(VkDescriptorPoolSize{
        .type = Ratio.Type,
        .descriptorCount = static_cast<uint32_t>(Ratio.Ratio * MaxSets)});
  }

  VkDescriptorPoolCreateInfo PoolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  PoolInfo.maxSets = MaxSets;

  PoolInfo.pPoolSizes = PoolSizes.data();
  PoolInfo.poolSizeCount = static_cast<uint32_t>(PoolSizes.size());

  PoolInfo.flags = 0;

  vkCreateDescriptorPool(Device, &PoolInfo, VK_NULL_HANDLE, &Pool);
}

void DescriptorAllocator::ClearDescriptors(VkDevice Device) {
  vkResetDescriptorPool(Device, Pool, 0);
}

void DescriptorAllocator::DestroyPool(VkDevice Device) {
  vkDestroyDescriptorPool(Device, Pool, VK_NULL_HANDLE);
}

VkDescriptorSet DescriptorAllocator::Allocate(VkDevice Device,
                                              VkDescriptorSetLayout Layout) {
  VkDescriptorSetAllocateInfo AllocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  AllocInfo.pNext = VK_NULL_HANDLE;
  AllocInfo.descriptorPool = Pool;
  AllocInfo.descriptorSetCount = 1;
  AllocInfo.pSetLayouts = &Layout;

  VkDescriptorSet DescriptorSet;
  VK_CHECK(vkAllocateDescriptorSets(Device, &AllocInfo, &DescriptorSet));

  return DescriptorSet;
}

