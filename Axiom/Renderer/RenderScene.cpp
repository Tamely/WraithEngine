#include "Renderer/RenderScene.h"

namespace Axiom {
void RenderScene::Reset() {
  FrameNumber = 0;
  CpuFrameMs = 0.0f;
  ActiveCamera = nullptr;
  BackgroundColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
  Submissions.clear();
  GizmoOverlay.reset();
  Sun.reset();
  LightBillboards.clear();
  SkyboxColorTop = glm::vec3(0.08f, 0.09f, 0.14f);
  SkyboxColorBottom = glm::vec3(0.14f, 0.24f, 0.38f);
  SkyboxHDRTexture.reset();
}
} // namespace Axiom
