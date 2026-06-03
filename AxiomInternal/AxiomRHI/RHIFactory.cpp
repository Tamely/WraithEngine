#include "RHI/RHIFactory.h"

#include "AxiomRHI/Vulkan/VulkanRhiDevice.h"

namespace Axiom {
std::unique_ptr<IRHIDevice> CreateRHIDevice(RendererBackendType BackendType) {
  switch (BackendType) {
  case RendererBackendType::Vulkan:
    return std::make_unique<VulkanRhiDevice>();
  }

  return nullptr;
}
} // namespace Axiom
