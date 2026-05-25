#pragma once

#include "Renderer/Material.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Axiom {
class VulkanMesh;

enum class MeshRenderPath {
  Graphics,
  Compute,
};

struct MeshVertex {
  glm::vec3 Position{0.0f, 0.0f, 0.0f};
  glm::vec3 Normal{0.0f, 0.0f, 1.0f};
  glm::vec2 TexCoord{0.0f, 0.0f};
};
static_assert(sizeof(MeshVertex) == 32);

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

struct MeshCreateOptions {
  bool KeepCpuData{false};
};

class Mesh {
public:
  virtual ~Mesh() = default;
};

using MeshRef = std::shared_ptr<Mesh>;

struct RenderMeshSubmissionDebugData {
  std::string Name;
};

using RenderMeshSubmissionDebugDataId = uint32_t;

RenderMeshSubmissionDebugDataId
RegisterRenderMeshSubmissionDebugData(RenderMeshSubmissionDebugData Data);
const RenderMeshSubmissionDebugData *
TryGetRenderMeshSubmissionDebugData(RenderMeshSubmissionDebugDataId Id);
std::string_view
GetRenderMeshSubmissionDebugName(RenderMeshSubmissionDebugDataId Id);

VulkanMesh *ResolveVulkanMesh(const MeshRef &Mesh);

struct RenderMeshSubmission {
  MeshRef Mesh;
  VulkanMesh *TypedMesh{nullptr};
  MaterialInstanceRef Material;
  RenderMeshSubmissionDebugDataId DebugDataId{0};
  MeshRenderPath RenderPath{MeshRenderPath::Graphics};
  glm::mat4 Transform{1.0f};
  bool Translucent{false};
};
} // namespace Axiom
