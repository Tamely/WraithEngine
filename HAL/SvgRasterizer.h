#pragma once

#include "Renderer/Material.h"

#include <cstdint>
#include <string_view>

namespace Axiom::HAL {
TextureSourceDataRef RasterizeSvg(std::string_view SvgText,
                                  uint32_t TargetSize);
} // namespace Axiom::HAL
