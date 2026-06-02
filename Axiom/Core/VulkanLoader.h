#pragma once

#include <string>

namespace Axiom {
struct VulkanLoaderInfo {
  void *ProcAddr{nullptr};
  std::string Source;
  bool UsesCustomLoader{false};
  bool IsAvailable{false};
};

[[nodiscard]] const char *GetPlatformName();
[[nodiscard]] const VulkanLoaderInfo &GetVulkanLoaderInfo();
void ConfigureGlfwVulkanLoader();
} // namespace Axiom
