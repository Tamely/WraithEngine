#pragma once

#include <glm/vec4.hpp>

#include <vector>

namespace Axiom {
class Camera;

struct RenderSubmission {
  void *Mesh{nullptr};
  void *Material{nullptr};
};

class RenderScene {
public:
  void Reset();

  Camera *ActiveCamera{nullptr};
  glm::vec4 BackgroundColor{1.0f, 0.0f, 0.0f, 1.0f};
  std::vector<RenderSubmission> Submissions;
};
} // namespace Axiom
