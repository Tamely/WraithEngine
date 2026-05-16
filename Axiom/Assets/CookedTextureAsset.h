#pragma once

#include "Renderer/Material.h"
#include "Session/SessionTypes.h"

#include <filesystem>
#include <optional>

namespace Axiom::Assets {

constexpr uint32_t kCookedTextureFormatVersion = 2;

enum class CookedTexturePixelFormat : uint32_t {
  RGBA8 = 0,
  RGBA32F = 1,
};

bool SaveCookedTextureAsset(const std::filesystem::path &Path,
                            const TextureSourceData &Texture, AssetId Asset);

std::optional<TextureSourceData>
LoadCookedTextureAsset(const std::filesystem::path &Path);

bool SaveCookedHDRTextureAsset(const std::filesystem::path &Path,
                               const HDRTextureSourceData &Texture,
                               AssetId Asset);

std::optional<HDRTextureSourceData>
LoadCookedHDRTextureAsset(const std::filesystem::path &Path);

} // namespace Axiom::Assets
