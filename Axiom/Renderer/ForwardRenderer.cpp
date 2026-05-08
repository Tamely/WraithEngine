#include "Renderer/ForwardRenderer.h"

#include "Renderer/RenderScene.h"
#include "Renderer/RendererBackend.h"

namespace Axiom {
void ForwardRenderer::Init(RendererBackend *Backend) { m_Backend = Backend; }

void ForwardRenderer::Shutdown() { m_Backend = nullptr; }

void ForwardRenderer::Render(RenderScene &Scene) {
  if (!Scene.Submissions.empty()) {
    m_Backend->RenderSceneMeshes(Scene);
    return;
  }

  m_Backend->RenderFallbackBackground(Scene);
}
} // namespace Axiom
