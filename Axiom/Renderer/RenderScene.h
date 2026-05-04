#pragma once

#include "Renderer/Mesh.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace Axiom {
class Camera;

class RenderScene {
public:
  void Reset();

  const Camera *ActiveCamera{nullptr};
  glm::vec4 BackgroundColor{1.0f, 0.0f, 0.0f, 1.0f};
  std::vector<RenderMeshSubmission> Submissions;
};
} // namespace Axiom
