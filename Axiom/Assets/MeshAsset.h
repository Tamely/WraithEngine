#pragma once

#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {
std::optional<MeshSceneData> LoadBasicMeshAsset(const std::filesystem::path &Path);
}
