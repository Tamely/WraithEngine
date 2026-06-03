#pragma once

#include "RHI/IRHI.h"

#include <memory>

namespace Axiom {
std::unique_ptr<IRHIDevice> CreateRHIDevice(RendererBackendType BackendType);
} // namespace Axiom
