#pragma once

#include "Renderer/Vulkan/VulkanTypes.h"

struct DescriptorLayoutBuilder {
  std::vector<VkDescriptorSetLayoutBinding> Bindings;

  void AddBinding(uint32_t Binding, VkDescriptorType Type);
  void Clear();
  VkDescriptorSetLayout Build(VkDevice Device, VkShaderStageFlags ShaderStages,
                              void *pNext = VK_NULL_HANDLE,
                              VkDescriptorSetLayoutCreateFlags Flags = 0);
};

struct DescriptorAllocator {
  struct PoolSizeRatio {
    VkDescriptorType Type;
    float Ratio;
  };

  VkDescriptorPool Pool;

  void InitPool(VkDevice Device, uint32_t MaxSets,
                std::span<PoolSizeRatio> PoolRatios);
  void ClearDescriptors(VkDevice Device);
  void DestroyPool(VkDevice Device);

  VkDescriptorSet Allocate(VkDevice Device, VkDescriptorSetLayout Layout);
};

