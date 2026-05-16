#include "Renderer/RenderCommand.h"

namespace Axiom {
RenderScene *RenderCommand::s_ActiveScene = nullptr;

void RenderCommand::BeginScene(RenderScene &Scene) {
  Scene.Reset();
  s_ActiveScene = &Scene;
}

void RenderCommand::SetCamera(const Camera &Camera) {
  if (s_ActiveScene) {
    s_ActiveScene->ActiveCamera = &Camera;
  }
}

void RenderCommand::Submit(const RenderMeshSubmission &Submission) {
  if (s_ActiveScene) {
    s_ActiveScene->Submissions.push_back(Submission);
  }
}

void RenderCommand::SubmitLightBillboard(
    const LightBillboardOverlay &Billboard) {
  if (s_ActiveScene) {
    s_ActiveScene->LightBillboards.push_back(Billboard);
  }
}

void RenderCommand::SetGizmoOverlay(const GizmoOverlayData &Gizmo) {
  if (s_ActiveScene) {
    s_ActiveScene->GizmoOverlay = Gizmo;
  }
}

void RenderCommand::SetSun(const DirectionalLight &Light) {
  if (s_ActiveScene) {
    s_ActiveScene->Sun = Light;
  }
}

void RenderCommand::SetSkyboxColors(const glm::vec3 &Top, const glm::vec3 &Bottom) {
  if (s_ActiveScene) {
    s_ActiveScene->SkyboxColorTop = Top;
    s_ActiveScene->SkyboxColorBottom = Bottom;
  }
}

void RenderCommand::EndScene() { s_ActiveScene = nullptr; }
} // namespace Axiom
