#pragma once

#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Axiom::Assets {

constexpr uint32_t kCookedMeshFormatVersion = 1;

struct CookedMeshSceneData {
  struct InstanceData {
    std::string Name;
    MeshData Mesh;
    glm::mat4 Transform{1.0f};
  };

  std::vector<InstanceData> Instances;
};

bool SaveCookedMeshAsset(const std::filesystem::path &Path,
                         const CookedMeshSceneData &Scene,
                         AssetId Asset);

std::optional<CookedMeshSceneData>
LoadCookedMeshAsset(const std::filesystem::path &Path);

CookedMeshSceneData ToCookedMeshSceneData(const MeshSceneData &Scene);
MeshSceneData ToRuntimeMeshSceneData(const CookedMeshSceneData &Scene);

} // namespace Axiom::Assets
