#include "Renderer/Vulkan/VulkanPipeline.h"
#include "Renderer/Vulkan/VulkanInitializers.h"

#include <fstream>

namespace VkUtil {
bool LoadShaderModule(const char *FilePath, VkDevice Device,
                      VkShaderModule *OutShaderModule) {
  // Open the file with the cursor at the end
  std::ifstream File(FilePath, std::ios::ate | std::ios::binary);
  if (!File.is_open()) {
    return false;
  }

  // Get the size of the file (since it's at the end, it's just the pos)
  size_t FileSize = static_cast<size_t>(File.tellg());

  // SpirV expects the buffer to be uint32[], so calc the size to that
  std::vector<uint32_t> Buffer(FileSize / sizeof(uint32_t));

  // Read the data into the buffer
  File.seekg(0);
  File.read((char *)Buffer.data(), FileSize);
  File.close();

  // Create a new shader module using the buffer we loaded
  VkShaderModuleCreateInfo ShaderCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE};

  // codeSize has to be in bytes so multiply the ints in the buffer by the size
  // of uint32 to know the real size
  ShaderCreateInfo.codeSize = Buffer.size() * sizeof(uint32_t);
  ShaderCreateInfo.pCode = Buffer.data();

  // Check that the creation is valid
  VkShaderModule ShaderModule;
  if (vkCreateShaderModule(Device, &ShaderCreateInfo, VK_NULL_HANDLE,
                           &ShaderModule) != VK_SUCCESS) {
    return false;
  }
  *OutShaderModule = ShaderModule;
  return true;
}
} // namespace VkUtil

