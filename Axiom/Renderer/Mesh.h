#pragma once

#include "Renderer/Material.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace Axiom {
enum class MeshRenderPath {
  Graphics,
  Compute,
};

struct MeshVertex {
  glm::vec4 Position{0.0f, 0.0f, 0.0f, 1.0f};
  glm::vec4 Normal{0.0f, 0.0f, 1.0f, 0.0f};
  glm::vec2 TexCoord{0.0f, 0.0f};
};

struct MeshData {
  std::vector<MeshVertex> Vertices;
  std::vector<uint32_t> Indices;
  glm::vec3 BoundsMin{0.0f};
  glm::vec3 BoundsMax{0.0f};
};

struct MeshSceneData {
  struct MeshInstanceData {
    std::string Name;
    MeshData Mesh;
    MaterialInstanceRef Material;
    glm::mat4 Transform{1.0f};
  };

  std::vector<MeshInstanceData> Instances;
};

struct MeshSceneLoadOptions {
  MeshRenderPath DefaultRenderPath{MeshRenderPath::Graphics};
  std::unordered_set<std::string> ComputeMeshNames;
};

class Mesh {
public:
  virtual ~Mesh() = default;
};

using MeshRef = std::shared_ptr<Mesh>;

struct RenderMeshSubmission {
  MeshRef Mesh;
  MaterialInstanceRef Material;
  std::string Name;
  MeshRenderPath RenderPath{MeshRenderPath::Graphics};
  glm::mat4 Transform{1.0f};
  bool Translucent{false};
};
} // namespace Axiom
