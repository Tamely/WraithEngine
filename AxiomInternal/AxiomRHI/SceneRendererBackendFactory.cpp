#include "AxiomRHI/SceneRendererBackendFactory.h"

#include "AxiomRHI/Vulkan/VulkanSceneRenderer.h"

namespace Axiom {
std::unique_ptr<ISceneRendererBackend>
CreateSceneRendererBackend(IRHIDevice &Device, RendererBackendType BackendType) {
  (void)Device;

  switch (BackendType) {
  case RendererBackendType::Vulkan:
    return std::make_unique<VulkanSceneRenderer>();
  }

  return nullptr;
}
} // namespace Axiom
