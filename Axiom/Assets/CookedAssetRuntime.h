#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {

std::optional<std::filesystem::path>
FindContentRootForPath(const std::filesystem::path &Path);

std::optional<MeshSceneData>
LoadCookedMeshAssetIfAvailable(const std::filesystem::path &Path);

TextureSourceDataRef
LoadCookedTextureAssetIfAvailable(const std::filesystem::path &Path);

} // namespace Axiom::Assets
