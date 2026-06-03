#pragma once

#include "AxiomRHI/Vulkan/VulkanTypes.h"

namespace VkUtil {
bool LoadShaderModule(const char *FilePath, VkDevice Device,
                      VkShaderModule *OutShaderModule);
}

