#pragma once

namespace Axiom {
class RendererBackend;
class RenderScene;

class RenderTechnique {
public:
  virtual ~RenderTechnique() = default;

  virtual void Init(RendererBackend *Backend) = 0;
  virtual void Shutdown() = 0;
  virtual void Render(RenderScene &Scene) = 0;
};
} // namespace Axiom
