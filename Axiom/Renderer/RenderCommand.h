#pragma once

#include "Renderer/RenderScene.h"

namespace Axiom {
class Camera;

class RenderCommand {
public:
  static void BeginScene(RenderScene &Scene);
  static void SetCamera(const Camera &Camera);
  static void Submit(const RenderMeshSubmission &Submission);
  static void SubmitLightBillboard(const LightBillboardOverlay &Billboard);
  static void SetGizmoOverlay(const GizmoOverlayData &Gizmo);
  static void SetSun(const DirectionalLight &Light);
  static void EndScene();

private:
  static RenderScene *s_ActiveScene;
};
} // namespace Axiom
