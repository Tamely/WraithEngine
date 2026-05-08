#pragma once

#include "Renderer/VideoEncoding.h"

#include <memory>

namespace Axiom {
std::unique_ptr<IVideoEncoder> CreateDefaultVideoEncoder();
} // namespace Axiom
