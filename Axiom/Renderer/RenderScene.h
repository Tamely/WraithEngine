#pragma once

#include "Renderer/Mesh.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <optional>
#include <vector>

namespace Axiom {
class Camera;

struct GizmoOverlayData {
  glm::vec3 WorldPosition{0.0f};
  float Scale{0.5f};
};

class RenderScene {
public:
  void Reset();

  const Camera *ActiveCamera{nullptr};
  glm::vec4 BackgroundColor{1.0f, 0.0f, 0.0f, 1.0f};
  std::vector<RenderMeshSubmission> Submissions;
  std::optional<GizmoOverlayData> GizmoOverlay;
};
} // namespace Axiom
