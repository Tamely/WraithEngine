#pragma once

#include "Renderer/RendererBackend.h"

#include <memory>

namespace Axiom {
std::unique_ptr<RendererBackend>
CreateRendererBackend(RendererBackendType BackendType);
} // namespace Axiom
