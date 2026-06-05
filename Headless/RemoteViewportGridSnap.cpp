#include "RemoteViewportGridSnap.h"

#include <algorithm>
#include <cmath>

namespace Axiom {
namespace {
float SnapToStep(float Value, float Step) {
  if (Step <= 0.0f) {
    return Value;
  }
  return std::round(Value / Step) * Step;
}
} // namespace

void RemoteViewportGridSnap::Sanitize(GridSnapSettings &Settings) const {
  Settings.TranslationStep = std::max(MinimumScale, Settings.TranslationStep);
  Settings.RotationStepDegrees = std::max(0.001f, Settings.RotationStepDegrees);
  Settings.ScaleStep = std::max(MinimumScale, Settings.ScaleStep);
}

void RemoteViewportGridSnap::Apply(const GridSnapSettings &Settings,
                                   GizmoMode Mode, int Axis,
                                   glm::vec3 &Location,
                                   glm::vec3 &RotationDegrees,
                                   glm::vec3 &Scale) const {
  if (!Settings.Enabled || Axis < 0 || Axis > 2) {
    return;
  }

  switch (Mode) {
  case GizmoMode::Translate:
    Location[Axis] = SnapToStep(Location[Axis], Settings.TranslationStep);
    break;
  case GizmoMode::Rotate:
    RotationDegrees[Axis] =
        SnapToStep(RotationDegrees[Axis], Settings.RotationStepDegrees);
    break;
  case GizmoMode::Scale:
    Scale[Axis] =
        std::max(MinimumScale, SnapToStep(Scale[Axis], Settings.ScaleStep));
    break;
  }
}
} // namespace Axiom
