#include "Renderer/ForwardRenderer.h"

#include "Renderer/RenderScene.h"
#include "Renderer/RendererBackend.h"

namespace Axiom {
std::string_view ForwardRenderer::GetName() const { return "Forward"; }

void ForwardRenderer::Init(RendererBackend &Backend) { m_Backend = &Backend; }

void ForwardRenderer::Shutdown() { m_Backend = nullptr; }

void ForwardRenderer::Render(RenderScene &Scene) {
  if (!Scene.Submissions.empty()) {
    m_Backend->PrepareSceneFrame(Scene);
    m_Backend->RecordBackground();
    m_Backend->RecordDepthPrepass();
    m_Backend->BuildHzb();
    m_Backend->RecordComputeMeshPath();
    m_Backend->RecordOpaqueForward();
    m_Backend->RecordTranslucentForward();
    m_Backend->FinalizeSceneFrame();
    return;
  }

  m_Backend->RenderFallbackBackground(Scene);
}
} // namespace Axiom
