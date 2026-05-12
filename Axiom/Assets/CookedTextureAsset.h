#pragma once

#include "Renderer/Material.h"
#include "Session/SessionTypes.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {

constexpr uint32_t kCookedTextureFormatVersion = 1;

bool SaveCookedTextureAsset(const std::filesystem::path &Path,
                            const TextureSourceData &Texture, AssetId Asset);

std::optional<TextureSourceData>
LoadCookedTextureAsset(const std::filesystem::path &Path);

} // namespace Axiom::Assets
