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
[[nodiscard]] bool CanInitializeHeadlessVulkan();
void ConfigureGlfwVulkanLoader();
} // namespace Axiom
