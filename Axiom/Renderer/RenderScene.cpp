#include "Renderer/RenderScene.h"

namespace Axiom {
void RenderScene::Reset() {
  ActiveCamera = nullptr;
  BackgroundColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
  Submissions.clear();
  GizmoOverlay.reset();
  Sun.reset();
}
} // namespace Axiom
