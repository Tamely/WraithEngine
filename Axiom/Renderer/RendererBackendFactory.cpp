#include "Renderer/RendererBackendFactory.h"

#include "Renderer/Vulkan/VulkanRendererBackend.h"

namespace Axiom {
std::unique_ptr<RendererBackend>
CreateRendererBackend(RendererBackendType BackendType) {
  switch (BackendType) {
  case RendererBackendType::Vulkan:
    return std::make_unique<VulkanRendererBackend>();
  }

  return nullptr;
}
} // namespace Axiom
