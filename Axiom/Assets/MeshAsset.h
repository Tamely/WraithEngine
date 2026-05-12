#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {
std::optional<MeshSceneData> LoadBasicMeshAsset(const std::filesystem::path &Path);
std::optional<MeshSceneData>
LoadBasicMeshAssetFromSource(const std::filesystem::path &Path);
TextureSourceDataRef LoadTextureFromFile(const std::filesystem::path &Path);
TextureSourceDataRef LoadTextureFromMemory(const unsigned char *Bytes, int Length,
                                           const std::string &DebugName);
} // namespace Axiom::Assets
