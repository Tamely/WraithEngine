#pragma once

#include <Renderer/RenderScene.h>

#include <glm/vec3.hpp>

namespace Axiom {
struct GridSnapSettings {
  bool Enabled{true};
  float TranslationStep{1.0f};
  float RotationStepDegrees{15.0f};
  float ScaleStep{0.1f};
};

class RemoteViewportGridSnap {
public:
  static constexpr float MinimumScale = 0.001f;

  void Sanitize(GridSnapSettings &Settings) const;
  void Apply(const GridSnapSettings &Settings, GizmoMode Mode, int Axis,
             glm::vec3 &Location, glm::vec3 &RotationDegrees,
             glm::vec3 &Scale) const;
};
} // namespace Axiom
