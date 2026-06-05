#pragma once

#include "Renderer/Material.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Axiom {
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
    std::shared_ptr<MaterialInstance> Material;
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

struct MeshHandle {
  uint64_t Value{0};

  [[nodiscard]] bool IsValid() const { return Value != 0; }
  auto operator<=>(const MeshHandle &) const = default;
};

struct MeshHandleHash {
  size_t operator()(const MeshHandle &Handle) const noexcept {
    return std::hash<uint64_t>{}(Handle.Value);
  }
};

class Mesh {
public:
  virtual ~Mesh() = default;

  [[nodiscard]] MeshHandle GetHandle() const { return m_Handle; }
  [[nodiscard]] bool HasHandle() const { return m_Handle.IsValid(); }
  void AssignHandle(MeshHandle Handle) { m_Handle = Handle; }

private:
  MeshHandle m_Handle{};
};

using MeshRef = std::shared_ptr<Mesh>;

MeshHandle GetMeshHandle(const MeshRef &Mesh);

struct RenderMeshResource {
  MeshHandle Handle{};
  MeshRef Mesh;

  [[nodiscard]] bool IsValid() const {
    return Handle.IsValid() && Mesh != nullptr;
  }
};

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

struct RenderMeshSubmission {
  MeshHandle MeshHandle{};
  // Material handles are resolved through the renderer-owned material registry.
  // Producers must keep the registered material valid for the frame in which
  // this submission is consumed.
  MaterialHandle MaterialHandle{};
  RenderMeshSubmissionDebugDataId DebugDataId{0};
  MeshRenderPath RenderPath{MeshRenderPath::Graphics};
  glm::mat4 Transform{1.0f};
  bool Translucent{false};
};

struct LoadedMeshScene {
  std::vector<RenderMeshResource> Resources;
  std::vector<RenderMeshSubmission> Submissions;
};
} // namespace Axiom
