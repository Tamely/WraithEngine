#pragma once

#include <cstdint>
#include <string_view>

namespace Axiom {
class RendererBackend;
class RenderScene;

class RenderTechnique {
public:
  struct AttachmentRequirements {
    bool NeedsGBuffer{false};
    uint32_t GBufferColorTargetCount{0};

    constexpr bool operator==(const AttachmentRequirements &) const = default;
  };

  virtual ~RenderTechnique() = default;

  virtual std::string_view GetName() const = 0;
  virtual void Init(RendererBackend &Backend) = 0;
  virtual void Shutdown() = 0;
  virtual void Render(RenderScene &Scene) = 0;
  virtual AttachmentRequirements GetAttachmentRequirements() const {
    return {};
  }
};
} // namespace Axiom
