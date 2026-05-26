#pragma once

#include "Renderer/RenderTechnique.h"

namespace Axiom {
class ForwardRenderer final : public RenderTechnique {
public:
  std::string_view GetName() const override;
  void Init(RendererBackend &Backend) override;
  void Shutdown() override;
  void Render(RenderScene &Scene) override;

private:
  RendererBackend *m_Backend{nullptr};
};
} // namespace Axiom
