#pragma once

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include <volk.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "Core/Log.h"
#include "Renderer/Vulkan/VulkanStringUtils.h"

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      A_CORE_ERROR("Detected Vulkan error: {0}", VkResultToString(err));       \
      Axiom::Log::Flush();                                                     \
      abort();                                                                 \
    }                                                                          \
  } while (0)

struct AllocatedImage {
  VkImage Image;
  VkImageView ImageView;
  VmaAllocation Allocation;
  VkExtent3D ImageExtent;
  VkFormat ImageFormat;
};

struct AllocatedBuffer {
  VkBuffer Buffer{VK_NULL_HANDLE};
  VmaAllocation Allocation{VK_NULL_HANDLE};
  VmaAllocationInfo Info{};
  VkDeviceSize Size{0};
};
