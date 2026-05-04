#include "Renderer/RenderCommand.h"

namespace Axiom {
RenderScene *RenderCommand::s_ActiveScene = nullptr;

void RenderCommand::BeginScene(RenderScene &Scene) {
  Scene.Reset();
  s_ActiveScene = &Scene;
}

void RenderCommand::Submit(const RenderSubmission &Submission) {
  if (s_ActiveScene) {
    s_ActiveScene->Submissions.push_back(Submission);
  }
}

void RenderCommand::EndScene() { s_ActiveScene = nullptr; }
} // namespace Axiom
