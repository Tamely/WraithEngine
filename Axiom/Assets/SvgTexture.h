#pragma once

#include "Renderer/Material.h"

#include <filesystem>

namespace Axiom::Assets {
TextureSourceDataRef LoadSvgTextureFromFile(const std::filesystem::path &Path,
                                            uint32_t TargetSize = 128);
} // namespace Axiom::Assets
