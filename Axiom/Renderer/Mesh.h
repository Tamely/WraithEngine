#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace Axiom {
struct MeshVertex {
  glm::vec4 Position{0.0f, 0.0f, 0.0f, 1.0f};
  glm::vec4 Normal{0.0f, 0.0f, 1.0f, 0.0f};
};

struct MeshData {
  std::vector<MeshVertex> Vertices;
  std::vector<uint32_t> Indices;
  glm::vec3 BoundsMin{0.0f};
  glm::vec3 BoundsMax{0.0f};
};

class Mesh {
public:
  virtual ~Mesh() = default;
};

using MeshRef = std::shared_ptr<Mesh>;

struct RenderMeshSubmission {
  MeshRef Mesh;
  glm::mat4 Transform{1.0f};
};
} // namespace Axiom
