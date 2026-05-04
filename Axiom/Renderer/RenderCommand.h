#pragma once

#include "Renderer/RenderScene.h"

namespace Axiom {
class RenderCommand {
public:
  static void BeginScene(RenderScene &Scene);
  static void Submit(const RenderSubmission &Submission);
  static void EndScene();

private:
  static RenderScene *s_ActiveScene;
};
} // namespace Axiom
