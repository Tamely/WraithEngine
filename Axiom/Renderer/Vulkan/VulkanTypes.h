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
#include <vulkan/vk_enum_string_helper.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "Core/Log.h"

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      A_CORE_ERROR("Detected Vulkan error: {0}", string_VkResult(err));        \
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

