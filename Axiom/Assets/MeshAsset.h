#pragma once

#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {
std::optional<MeshData> LoadBasicMeshAsset(const std::filesystem::path &Path);
}
